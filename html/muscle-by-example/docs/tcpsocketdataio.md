# muscle::TCPSocketDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html)

```#include "dataio/TCPSocketDataIO.h"```

[TCPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html) handles transmitting and receiving data over a TCP connection.

* The default implementation of the [CreateDataIO(const ConstSocketRef &)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractReflectSession.html#a002af36339ba9313a918f0621ad657a6) method in the session classes ([AbstractReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractReflectSession.html) and its subclasses) creates a [TCPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html) object.
* [TCPSocketDataIO::FlushOutput()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html#a6d08a6d7fdc42d434376004aab2c229f) is implemented to force pending outgoing data across the TCP socket ASAP.  It is called by the gateway objects when they are done sending data for the time being, to avoid any Nagle-induced delay.
* [TCPSocketDataIO::GetOutputStallLimit()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html#ae7567d1f2ef227988c259b11b92bef8d) returns 180,000,000 microseconds (aka 3 minutes) by default, but can be set to another timeout value if you prefer.  That will control how much time elapses before a "stalled" session (whose client has stopped reading data, but hasn't disconnected) is abandoned.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [dataio/example_2_tcp_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_2_tcp_server.cpp)
