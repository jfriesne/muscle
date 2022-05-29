# muscle::WaitCondition class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html)

```#include "system/WaitCondition.h"```

MUSCLE's WaitCondition class

* A [WaitCondition](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html) is a simple, lightweight thread-notification mechanism.
* A thread can call [Wait()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html#a66dbbdace3a0c047ca659df9f236f6ca) on the [WaitCondition](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html), and [Wait()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html#a66dbbdace3a0c047ca659df9f236f6ca) won't return until another thread calls [Notify()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html#a2cab103f8544991f41d034879647d52e)  on it (or optionally until a timeout-time is reached).
* Any calls to [Notify()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html#a2cab103f8544991f41d034879647d52e) that happen at a time when no thread is waiting on the [WaitCondition](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html) will be cached, and will cause the next call to [Wait()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html#a66dbbdace3a0c047ca659df9f236f6ca) to return immediately.
* Similar to:  [std::condition_variable](https://en.cppreference.com/w/cpp/thread/condition_variable), [QWaitCondition](http://doc.qt.io/qt-5/qwaitcondition.html), [SleepConditionVariableCS](https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablecs)
* When compiled with C++11, [WaitCondition](https://public.msli.com/lcs/muscle/html/classmuscle_1_1WaitCondition.html) is implemented via `std::condition_variable`.  Otherwise, it will be implemented using an appropriate threading API (i.e. `pthreads` or an OS-specific API).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/waitcondition` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [waitcondition/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/waitcondition/example_1_basic_usage.cpp)
* [waitcondition/example_2_ping_pong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/waitcondition/example_2_ping_pong.cpp)
