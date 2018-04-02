# muscle::AtomicCounter class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicCounter.html)

```#include "system/AtomicCounter.h"```

* Thread-safe (lockless) counter type
* Similar to:  `std::atomic<int32>`, `InterlockedIncrement()/InterlockedDecrement()`, `OSAtomicIncrement32Barrier()/OSAtomicDecrement32Barrier()`
* Used primarily by the `Ref/RefCountable` classes for implementing thread-safe shared pointers
* Constructor initializes the counter to zero
* `AtomicIncrement()` does an atomic-increment of the counter value, and returns true iff the counter's post-increment value is 1.
* `AtomicDecrement()` does an atomic-decrement of the counter value, and returns true iff the counter's post-decrement value is 0.
* Compiles down to a non-atomic/plain-int32 counter if `-DMUSCLE_SINGLE_THREAD_ONLY` is defined (i.e. if multithreading support is disabled)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/atomiccounter` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [atomiccounter/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/atomiccounter/example_1_basic_usage.cpp)
* [atomiccounter/example_2_stress_test.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/atomiccounter/example_2_stress_test.cpp)
