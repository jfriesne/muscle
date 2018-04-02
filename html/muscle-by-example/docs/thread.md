# muscle::Thread class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Thread.html)

```#include "system/Thread.h"```

* A `Thread` object holds a captive internal thread, plus two low-latency/zero-copy MessageRef queues (one for the main/owning thread to send command `Message` objects to the internal thread, and a second one for the internal thread to send reply `Message` objects back to the main/owning thread)
* Similar to:  `std::thread`, `pthread_t`, `QThread`, `_beginthreadex`
* When compiled with C++11, `Thread` is implemented via `std::thread`.  Otherwise, it will be implemented using an appropriate threading API (i.e. `pthreads` or an OS-specific API).
* Call `StartInternalThread()` to start the internal thread running
* Call `ShutdownInternalThread()` to tell the internal thread to go away (`ShutdownInternalThread()` will, by default, wait until the thread exits before returning)
* Call `WaitForInternalThreadToExit()` to block until the internal thread has exited (similar to `pthread_join`) (only necessary if you passed `false` as an argument to `ShutdownInternalThread()`, of course)
* Call `SendMessageToInternalThread()` to send a command Message to the internal thread, i.e. to tell it to wake up and to do something
* Call `GetNextReplyFromInternalThread()` to receive a reply Message back from the internal thread (second argument can be used to block waiting for the `Message`, if desired)
* The `ConstSocketRef` returned by `GetOwnerWakeupSocket()` can be used (in conjunction with `select()/SignalMultiplexer` (etc) as a way to wake up when the next reply `Message` is available from the internal thread.
* The default implementation of `Thread::InternalThreadEntry()` runs a while-loop around calls to `WaitForNextMessageFromOwner()` and `MessageReceivedFromOwner()`, so if you just need the internal thread to react to `Message` commands you send it, you need only override the `Thread::MessageReceivedFromOwner()` callback method.
* If you need more, you are free to override `Thread::InternalThreadEntry()` to whatever you need the internal thread to do.
* Note that `ShutdownInternalThread()` will signal the internal thread that it is time to exit by sending it a NULL `MessageRef` as a command.
* Thread priority can be set via `SetThreadPriority()`
* Thread stack size can be set via `SetThreadStackSize()` (must be done before calling `StartInternalThread()`)
* Another example can be seen in [this demonstration of how to add multithreading to a MUSCLE server](https://public.msli.com/lcs/muscle/MUSCLE-thread-demo1.04.zip)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/thread` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [thread/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/thread/example_1_basic_usage.cpp)
* [thread/example_2_dumb_server_with_thread.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/thread/example_2_dumb_server_with_thread.cpp)
