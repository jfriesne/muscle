# muscle::ReaderWriterMutex class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html)

```#include "system/ReaderWriterMutex.h"```

A recursive reader/writer mutex, useful for synchronization in multithreaded programs where you want to allow shared/read-only access to multiple "reader threads", and also sometimes provide exclusive/read-write access to one "writer thread" at a time.

* Similar to:  [std::shared_mutex](http://en.cppreference.com/w/cpp/thread/shared_mutex), [pthread_rwlock](https://pubs.opengroup.org/onlinepubs/009604599/functions/pthread_rwlock_rdlock.html), [QReadWriteLock](https://doc.qt.io/qt-6/qreadwritelock.html), [Win32 Slim Reader/Writer Locks](https://learn.microsoft.com/en-us/windows/win32/sync/slim-reader-writer--srw--locks)
* [LockReadOnly()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aeb2aa229e4bbb76f4f59b5c0e890648f) method locks the ReaderWriterMutex in shared/read-only mode, [UnlockReadOnly()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aa9bd69012df975268ea99d63895e9fff) unlocks it.
* [LockReadWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aa0baf1d741d49f36363534f19501ec33) method locks the ReaderWriterMutex in exclusive/read-write mode, [UnlockReadWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#a48e7a7849589e81b2b5e5bad1650a78b) unlocks it.
* Nested locking is okay (i.e. you can call [LockReadOnly()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aeb2aa229e4bbb76f4f59b5c0e890648f) or [LockReadWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aa0baf1d741d49f36363534f19501ec33) twice in a row, as long as you later call [UnlockReadOnly()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aa9bd69012df975268ea99d63895e9fff) or [UnlockReadWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#a48e7a7849589e81b2b5e5bad1650a78b) twice to unlock it)
* [ReadOnlyMutexGuard](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReadOnlyMutexGuard.html) can be placed on the stack; its constructor will call [LockReadOnly()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aeb2aa229e4bbb76f4f59b5c0e890648f), on the specified [ReaderWriterMutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html), and its destructor will call [UnlockReadOnly()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aa9bd69012df975268ea99d63895e9fff) (similar to `std::lock_guard`)
* [ReadWriteMutexGuard](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReadWriteMutexGuard.html) can be placed on the stack; its constructor will call [LockReadWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#aa0baf1d741d49f36363534f19501ec33), on the specified [ReaderWriterMutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html), and its destructor will call [UnlockReadWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html#a48e7a7849589e81b2b5e5bad1650a78b) (similar to `std::lock_guard`)
* [ReaderWriterMutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReaderWriterMutex.html) class compiles away to a no-op if `-DMUSCLE_SINGLE_THREAD_ONLY` is defined (i.e. if multithreading support is disabled)
* If `-DMUSCLE_ENABLE_DEADLOCK_FINDER` is defined, the locking and unlocking methods calls will log their calling patterns in a RAM buffer, and on process-exit, that data will be analyzed and diagnostics will be printed if any potential for deadlocks was detected.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/readerwritermutex` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [readerwritermutex/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/readerwritermutex/example_1_basic_usage.cpp)
* [readerwritermutex/example_2_mutex_guard.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/readerwritermutex/example_2_mutex_guard.cpp)