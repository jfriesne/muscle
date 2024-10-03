# muscle::Mutex class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html)

```#include "system/Mutex.h"```

A recursive mutex, useful for synchronization in multithreaded programs

* Similar to:  [std::recursive_mutex](http://en.cppreference.com/w/cpp/thread/recursive_mutex), [pthread_mutex_t](http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutex_lock.html), [QMutex](http://doc.qt.io/qt-5/qmutex.html), [CRITICAL_SECTION](https://msdn.microsoft.com/en-us/library/windows/desktop/ms682530(v=vs.85).aspx)
* When compiled with C++11 or later, it works as an inline wrapper around `std::recursive_mutex`.  Otherwise, it will be an inline wrapper around the appropriate native API.
* [Lock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#ae9ee0cb5ea32d42f2c1c072517cb30fa) method locks the Mutex, [Unlock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a9cc1d0496efe475f7012b6fb641a7725) unlocks it.
* [TryLock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a02e8c0192d29043201f4e1e8c73ecf05) tries to lock the Mutex, but will return an error code immediately if the Mutex is currently held by someone else.
* Nested locking is okay (i.e. you can call [Lock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#ae9ee0cb5ea32d42f2c1c072517cb30fa) twice in a row, as long as you later call [Unlock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a9cc1d0496efe475f7012b6fb641a7725) twice to unlock it)
* [MutexGuard](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MutexGuard.html) can be placed on the stack; its constructor will call [Lock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#ae9ee0cb5ea32d42f2c1c072517cb30fa), on the specified [Mutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html), and its destructor will call [Unlock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a9cc1d0496efe475f7012b6fb641a7725) (similar to `std::lock_guard`)
* [Mutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html) class compiles away to a no-op if `-DMUSCLE_SINGLE_THREAD_ONLY` is defined (i.e. if multithreading support is disabled)
* If `-DMUSCLE_ENABLE_DEADLOCK_FINDER` is defined, [Lock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#ae9ee0cb5ea32d42f2c1c072517cb30fa) and [Unlock()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html#a9cc1d0496efe475f7012b6fb641a7725) calls will log their calling patterns in a RAM buffer, and on process-exit, that data will be analyzed and diagnostics will be printed if any potential for deadlocks was detected.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/mutex` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [mutex/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/mutex/example_1_basic_usage.cpp)
* [mutex/example_2_mutex_guard.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/mutex/example_2_mutex_guard.cpp)
