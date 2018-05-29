# muscle::Mutex class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html)

```#include "system/Mutex.h"```

A recursive mutex, useful for synchronization in multithreaded programs

* Similar to:  [std::recursive_mutex](http://en.cppreference.com/w/cpp/thread/recursive_mutex), [pthread_mutex_t](http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutex_lock.html), [QMutex](http://doc.qt.io/qt-5/qmutex.html), [CRITICAL_SECTION](https://msdn.microsoft.com/en-us/library/windows/desktop/ms682530(v=vs.85).aspx)
* When compiled with C++11, it works as an inline wrapper around `std::recursive_mutex`.  Otherwise, it will be an inline wrapper around the appropriate native API.
* [Lock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a324f2ff27d1c4109bcc8e8b29ff05f2e) method locks the Mutex, [Unlock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a15bc71d43c2a343717ca6f05dd24e81f) unlocks it.
* Nested locking is okay (i.e. you can call [Lock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a324f2ff27d1c4109bcc8e8b29ff05f2e) twice in a row, as long as you later call [Unlock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a15bc71d43c2a343717ca6f05dd24e81f) twice to unlock it)
* [MutexGuard](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MutexGuard.html) can be placed on the stack; its constructor will call [Lock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a324f2ff27d1c4109bcc8e8b29ff05f2e), on the specified [Mutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html), and its destructor will call [Unlock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a15bc71d43c2a343717ca6f05dd24e81f) (similar to `std::lock_guard`)
* [Mutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html) class compiles away to a no-op if `-DMUSCLE_SINGLE_THREAD_ONLY` is defined (i.e. if multithreading support is disabled)
* If `-DMUSCLE_ENABLE_DEADLOCK_FINDER` is defined, [Lock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a324f2ff27d1c4109bcc8e8b29ff05f2e) and [Unlock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a15bc71d43c2a343717ca6f05dd24e81f) calls will leave breadcrumbs that can later be fed to the `tests/deadlockfinder.cpp` program, which will detect potential deadlocks in the locking patterns and tell you about them.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/mutex` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [mutex/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/mutex/example_1_basic_usage.cpp)
* [mutex/example_2_mutex_guard.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/mutex/example_2_mutex_guard.cpp)
