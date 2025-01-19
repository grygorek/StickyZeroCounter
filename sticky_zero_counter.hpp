// MIT License
//
// Copyright (c) 2025 Piotr Grygorczuk
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef WAITLESS_STICKY_COUNTER_H__
#define WAITLESS_STICKY_COUNTER_H__

#include <atomic>
#include <cstdint>

// Sticky counter is a thread safe and non-blocking, non waiting implementation of
// an atomic counter that once is decremented to 0, it will stay at 0.
//
// One and only one Decrement call will return true.
// Once Read returns 0, it will always return 0.
//
// The counter is unsigned 62bits wide.
//
// Preconditions:
//  * Counter is created initialized to 1.
//    Creating counter instace is considered the same as Increment has been called.
//  * Decrement must never be called without prior Increment.
//  * Overflowing the counter will lead to undefined behaviour.
class StickyCounter
{
  // Two MSBs are used to control the stickyness.
  static constexpr uint64_t is_zero = 1ull << 63;
  static constexpr uint64_t help = 1ull << 62;

public:
  // Returns true if has incremented the counter. Returns false if the counter is zero.
  constexpr bool Increment()
  {
    if (counter_.fetch_add(1) & is_zero)
      return false;
    return true;
  }

  // Returns true if has decremented the counter to 0. Only one decrement ever will return true.
  constexpr bool Decrement()
  {
    if (counter_.fetch_sub(1) == 1)
    {
      // Before we succeed setting is_zero, it may happen:
      //  * Increment is called and counter_ is no longer zero.
      //    The first exchange will fail and the 'help' won't be set. Return false so.
      //  * Read is called. It will see 0 and will set both is_zero and help.
      //    The first exchange will fail. The second condtion will see the 'help' and will
      //    try to take the ownership of decrementing to zero by setting is_zero.
      //    It may fail if another, concurrent Decrement already took the ownership.
      // Only one Decrement will succeed to clear the 'help' flag, and that is the one
      // that will take the ownership.
      uint64_t v = 0;
      if (counter_.compare_exchange_strong(v, is_zero))
        return true;
      else if ((v | help) && counter_.compare_exchange_strong(v, is_zero))
        return true;
    }
    return false;
  }

  constexpr uint64_t Read()
  {
    uint64_t v = counter_.load();

    // When Read sees 0, it means that some other thread is in the middle of the
    // Decrement call. But also, Read must never just simply return 0. If it does, then
    // there is a possibility that Increment is called in the meantime. The first Read
    // may return 0 but the consequtive Read may return 1. And that breaks the contract
    // of the sticky counter (once at 0, stay at 0). In order to prevent that, the Read
    // must help the Decrement to set is_zero. But setting just simple is_zero may also break
    // the contract that one and only one Decrement at some stage must return true.
    // If Read sets simple is_zero, none of the Decrements will ever return true.
    // 
    // Read must set the 'help' and is_zero flags. When Decrement sees the help flag,
    // it will try to clear it. One and only one Decrement will succeed clearing the flag and
    // that one will take the ownership of decrementing to zero.
    if (v == 0)
    {
      if (counter_.compare_exchange_strong(v, is_zero | help))
        return 0;
    }

    return (v & is_zero) ? 0 : v;
  }

private:
  std::atomic<uint64_t> counter_{1};
};

#endif // WAITLESS_STICKY_COUNTER_H__
