# Stick to Zero Counter
 Sticky counter is a thread safe and non-blocking, non waiting implementation of
 an atomic counter that once is decremented to 0, it will stay at 0.

 * One and only one Decrement call will return true.
 * Once Read returns 0, it will always return 0.

 The counter is unsigned 62bits wide.

* Preconditions:
   * Counter is created initialized to 1.
     Creating counter instace is considered the same as Increment has been called.
   *  Decrement must never be called without prior Increment.
   * Overflowing the counter will lead to undefined behaviour.

Inspired by: https://youtu.be/kPh8pod0-gk?si=tcf5K-jL9J0WF6mT
