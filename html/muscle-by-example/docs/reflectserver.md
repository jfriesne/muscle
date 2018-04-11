# muscle::ReflectServer class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html)

```#include "reflector/ReflectServer.h"```

[!["ReflectServer sessions diagram"](muscled_sessions.png)](muscled_sessions.png)

* `ReflectServer` implements a general-purpose event loop for MUSCLE components to run in.
* A typical MUSCLE server program would put a `ReflectServer` object on the stack in main(), add one or more `ServerComponent` objects to it, then call `ServerProcessLoop()` on the `ReflectServer`.
* `ServerProcessLoop()` will typically not return until it is time for the program to exit.
* `ServerProcessLoop()` uses a `SocketMultiplexer` internally to handle I/O operations and calls callback methods on the attached `ServerComponent` objects at the appropriate times.
* The `ReflectServer` also handles the role of a `PulseNodeManager`, with `ServerProcessLoop()` calling `Pulse()` on its attached objects at the time(s) they have requested to have their `Pulse()` method called.
* `ServerComponent` object subclasses include `ReflectSessionFactory` objects (for accepting incoming TCP connections) and `AbstractReflectSession` objects (for handling the I/O and state associated with a single file descriptor or socket)
* More complex MUSCLE programs might have multiple threads, many with a separate `ReflectServer` object implementing the thread's event loop.  (The `MessageTransceiverThread` class, in particular, does this)  A thread's `ReflectServer::ServerEventLoop()` call won't return until it is time for the thread to exit.
* New session objects can be added to the `ReflectServer` by calling `AddNewSession()`.
* New session objects that want to represent an outgoing TCP connection can be added to the `ReflectServer` by calling `AddNewConnectSession()`.  The TCP connect will be done asynchronously (using a non-blocking connect) so it won't hold off the event loop.
* New `ReflectServerFactory` objects can be added to the server by calling `PutAcceptFactory()`.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [reflector/example_1_dumb_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_1_dumb_server.cpp)
* [reflector/example_2_dumb_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_2_dumb_client.cpp)
* [reflector/example_3_annotated_dumb_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_3_annotated_dumb_server.cpp)
* [reflector/example_4_smart_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_4_smart_server.cpp)
* [reflector/example_5_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_5_smart_client.cpp)
* [reflector/example_6_smart_server_with_pulsenode.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_6_smart_server_with_pulsenode.cpp)
* [reflector/example_7_smart_server_with_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_7_smart_server_with_udp_pingpong.cpp)
