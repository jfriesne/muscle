# muscle::Mutex class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html)

```#include "system/Mutex.h"```

* Recursive Mutex, useful for synchronization in multithreaded programs
* Similar to:  `std::recursive_mutex`, `pthread_mutex_t`, `QMutex`, `CRITICAL_SECTION`
* When compiled with C++11, it works as an inline wrapper around `std::recursive_mutex`.  Otherwise, it will be an inline wrapper around the appropriate native API.
* `Lock()` method locks the Mutex, `Unlock()` unlocks it.
* Nested locking is okay (i.e. you can call `Lock()` twice in a row, as long as you later call `Unlock()` twice to unlock it)
* `MutexGuard` can be placed on the stack; its constructor will call `Lock()`, and its destructor will call `Unlock()` (similar to `std::lock_guard`)
* Mutex compiles away to a no-op if `-DMUSCLE_SINGLE_THREAD_ONLY` is defined (i.e. if multithreading support is disabled)
* If `-DMUSCLE_ENABLE_DEADLOCK_FINDER` is defined, `Lock()` and `Unlock()` calls will leave breadcrumbs that can later be fed to the `tests/deadlockfinder.cpp` program, which will detect potential deadlocks in the locking patterns and tell you about them.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/mutex` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [mutex/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/mutex/example_1_basic_usage.cpp)
* [mutex/example_2_mutex_guard.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/mutex/example_2_mutex_guard.cpp)
