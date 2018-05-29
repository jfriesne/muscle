# muscle::AtomicCounter class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicCounter.html)

```#include "system/AtomicCounter.h"```

A Thread-safe, lockless int32-counter

* Similar to:  [std::atomic&lt;int32&gt;](http://en.cppreference.com/w/cpp/atomic/atomic), [InterlockedIncrement()](https://msdn.microsoft.com/en-us/library/windows/desktop/ms683614(v=vs.85).aspx)/[InterlockedDecrement()](https://msdn.microsoft.com/en-us/library/windows/desktop/ms683580(v=vs.85).aspx)
* Used primarily by the [Ref](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Ref.html)/[RefCountable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RefCountable.html) classes for implementing thread-safe shared pointers
* Constructor initializes the counter to zero
* [AtomicIncrement()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicCounter.html#abfaf1ffb8afea7f355c71680fb35a693) does an atomic-increment of the counter value, and returns true iff the counter's post-increment value is 1.
* [AtomicDecrement()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicCounter.html#a912a1e8990ab05b13ad4934b91ecc00b) does an atomic-decrement of the counter value, and returns true iff the counter's post-decrement value is 0.
* Compiles down to a non-atomic/plain-int32 counter if `-DMUSCLE_SINGLE_THREAD_ONLY` is defined (i.e. if multithreading support is disabled)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/atomiccounter` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [atomiccounter/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/atomiccounter/example_1_basic_usage.cpp)
* [atomiccounter/example_2_stress_test.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/atomiccounter/example_2_stress_test.cpp)
