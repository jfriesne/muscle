# muscle::MessageTransceiverThread class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html)

```#include "system/MessageTransceiverThread.h"```

[MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html) object holds a captive internal thread, like the [Thread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Thread.html) class, except this thread's event loop is a full [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html) event loop.

* Many [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html)-style methods (e.g. [AddNewConnectSession()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html#a9be088ce383207dabf97610d1bb4a320), [PutAcceptFactory()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html#a40914524a0df1c888d0d23f7f9a54b32), etc are implemented in [MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html) as asynchronous calls to their like-named methods in the internal thread's [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html).
* The purpose of [MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html) is to allow your program to do its networking I/O in a separate thread, where it won't interfere with whatever your main thread is doing.
* Feedback from the internal thread is retrieved by calling the [GetNextEventFromInternalThread()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html#a86ba4307b6fc81171de0a2c797ffb86c) method.
* For Qt-based programs, see the [QMessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QMessageTransceiverThread.html) class, which subclasses this one and adds a Qt signals-and-slots API to make the events integration easier to use.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/rxtxthread` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [rxtxthread/example_1_threaded_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/rxtxthread/example_1_threaded_smart_client.cpp)
