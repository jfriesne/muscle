# muscle::StorageReflectSession class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html)

```#include "reflector/StorageReflectSession.h"```

[StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) implements the full MUSCLE server-side database and wildcard-based [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html)-forwarding functionality.

* Clients of a [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html) using [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) objects can post Messages into to their partition of the server-side [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) tree.
* Clients can also set up live-subscriptions so that they get notified whenever node of interest are created, deleted, or updated.
* Clients can include wildcard-patterns in a [PR_NAME_KEYS](https://public.msli.com/lcs/muscle/html/StorageReflectConstants_8h.html#a582a8aa80d5c0c9117446ec354ddc98c) String field of their outgoing Messages to indicate which other connected client(s) the server should forward the Messages to.
* See [The MUSCLE Beginner's Guide](https://public.msli.com/lcs/muscle/muscle/html/Beginners%20Guide.html) for more details.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [reflector/example_4_smart_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_4_smart_server.cpp)
* [reflector/example_5_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_5_smart_client.cpp)
* [reflector/example_6_smart_server_with_pulsenode.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_6_smart_server_with_pulsenode.cpp)
* [reflector/example_7_smart_server_with_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_7_smart_server_with_udp_pingpong.cpp)
