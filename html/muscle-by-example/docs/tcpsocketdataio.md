# muscle::TCPSocketDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html)

```#include "dataio/TCPSocketDataIO.h"```

* `TCPSocketDataIO` is used to handle transmitting and receiving data over a TCP socket.
* The default implementation of the `CreateDataIO(const ConstSocketRef &)` method in the session classes (`AbstractReflectSession` and its subclasses) creates a TCPSocketDataIO object.
* `TCPSocketDataIO::FlushOutput()` is implemented to force pending outgoing data across the TCP socket ASAP.  It is called by the gateway objects when they are done sending data for the time being, to avoid any Nagle-induced delay.
* `TCPSocketDataIO::GetOutputStallLimit()` returns 180,000,000 microseconds (aka 3 minutes) by default, but can be set to another timeout value if you prefer.  That will control how much time elapses before a "stalled" session (whose client has stopped reading data, but hasn't disconnected) is abandoned.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

* [dataio/example_2_tcp_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_2_tcp_server.cpp)
