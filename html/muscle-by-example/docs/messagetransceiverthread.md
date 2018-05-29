# muscle::MessageTransceiverThread class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html)

```#include "system/MessageTransceiverThread.h"```

[MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html) object holds a captive internal thread, like the [Thread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Thread.html) class, except this thread's event loop is a full [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html) event loop.

* Many [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html)-style methods (e.g. [AddNewConnectSession()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html#aaf7639eb2804af5d91e2152556008382), [PutAcceptFactory()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html#ab21f87e986617dc5bf325c205f958720), etc are implemented in [MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html) as asynchronous calls to their like-named methods in the internal thread's [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html).
* The purpose of [MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html) is to allow your program to do its networking I/O in a separate thread, where it won't interfere with whatever your main thread is doing.
* Feedback from the internal thread is retrieved by calling the [GetNextEventFromInternalThread()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html#a8aaee10a3afbb93be853d780814e857f) method.
* For Qt-based programs, see the [QMessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QMessageTransceiverThread.html) class, which subclasses this one and adds a Qt signals-and-slots API to make the events integration easier to use.
* Other [MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html) subclasses are available for other runtime environments (e.g. [SDLMessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SDLMessageTransceiverThread.html) for SDL-base programs, [Win32MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Win32MessageTransceiverThread.html) for WinAPI-based programs, etc)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/messagetransceiverthread` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [messagetransceiverthread/example_1_threaded_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/messagetransceiverthread/example_1_threaded_smart_client.cpp)
