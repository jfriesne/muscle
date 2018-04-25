# muscle::ReflectSessionFactory class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectSessionFactory.html)

```#include "reflector/AbstractReflectSessionFactory.h"```

[ReflectSessionFactory](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectSessionFactory.html) is a factory object that listens on a TCP port for incoming TCP connections.

* When an incoming TCP connection arrives, the factory's [CreateSession()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectSessionFactory.html#a8adb73b87a70de7945ce475ea3d59ca5) method is called.
* [CreateSession()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectSessionFactory.html#a8adb73b87a70de7945ce475ea3d59ca5)'s job is to create and return a new session object (e.g. a subclass of [AbstractReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractReflectSession.html)) that will be used to handle the new TCP connection's needs.
* If [CreateSession()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectSessionFactory.html#a8adb73b87a70de7945ce475ea3d59ca5) returns a valid reference, then the new session object is attached to the ReflectServer and used.  Or if [CreateSession()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectSessionFactory.html#a8adb73b87a70de7945ce475ea3d59ca5) returns a NULL reference, the incoming TCP connection is denied and the TCP socket closed.
* Existing subclasses of [ReflectSessionFactory](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectSessionFactory.html) include [StorageReflectSessionFactory](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSessionFactory.html) (creates [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) objects), [DumbReflectSessionFactory](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DumbReflectSessionFactory.html) (creates [DumbReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DumbReflectSession.html) objects), [FilterSessionFactory](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FilterSessionFactory.html) (proxy/decorator factory that allows/denies incoming TCP connections based on their source IP address), etc.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [reflector/example_1_dumb_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_1_dumb_server.cpp)
* [reflector/example_3_annotated_dumb_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_3_annotated_dumb_server.cpp)
* [reflector/example_4_smart_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_4_smart_server.cpp)
