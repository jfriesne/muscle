# muscle::MessageTransceiverThread class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html)

```#include "system/MessageTransceiverThread.h"```

* `MessageTransceiverThread` object holds a captive internal thread, like the `Thread` class, except this thread's event loop is a full `ReflectServer` event loop.
* Many `ReflectServer`-style methods (e.g. `AddNewConnectSession()`, `PutAcceptFactory()`, etc are implemented in `MessageTransceiverThread` as asynchronous calls to their like-named methods in the internal thread's `ReflectServer`.
* The purpose of `MessageTransceiverThread` is to allow you program to do full networking I/O in a separate thread, where it won't interfere with whatever your main thread is doing.
* Feedback from the internal thread is retrieved by calling the `GetNextEventFromInternalThread()` method.
* For Qt-based programs, see the `QMessageTransceiverThread` class, which subclasses this one and adds a Qt signals-and-slots API to make the events integration easier to use.
* Other `MessageTransceiverThread` subclasses are available for other runtime environments (e.g. `SDLMessageTransceiverThread` for SDL-base programs, `Win32MessageTransceiverThread` for WinAPI-based programs, etc)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/messagetransceiverthread` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [messagetransceiverthread/example_1_threaded_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/messagetransceiverthread/example_1_threaded_smart_client.cpp)
