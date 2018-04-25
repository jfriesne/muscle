# muscle::DumbReflectSession class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DumbReflectSession.html)

```#include "reflector/DumbReflectSession.h"```

[DumbReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DumbReflectSession.html) is a very simple concrete implemention of the [AbstractReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractReflectSession.html) session interface.

* [DumbReflectSession::MessageReceivedFromGateway()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DumbReflectSession.html#a627414e26cbf6869142f10f7f8f5e5bd) is implemented to forward the client's Message on to all the other session objects on the [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html).
* [DumbReflectSession::MessageReceivedFromSession()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DumbReflectSession.html#a077bf3e532fa0760b2ce3f0650f27d82) is implemented to forward the fellow-session's Message out to the session's own gateway (and thus on to the session's own client)
* In this way, a [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html) with a [DumbReflectSessionFactory](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DumbReflectSessionFactory.html) object installed becomes a simple one-to-many "chat room" style reflector, with any [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object sent by any client getting "reflected" to all the other connected clients (but not back to the sending client).
* This class is good for a simple chat server or a simple demonstration, but it isn't much used otherwise (except as the base class for the [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) class, where more sophisticated wildcard-based routing logic is implemented)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [reflector/example_1_dumb_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_1_dumb_server.cpp)
* [reflector/example_2_dumb_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_2_dumb_client.cpp)
* [reflector/example_3_annotated_dumb_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_3_annotated_dumb_server.cpp)
