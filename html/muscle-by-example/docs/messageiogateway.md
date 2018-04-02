# muscle::MessageIOGateway class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html)

```#include "iogateway/MessageIOGateway.h"```

* The `MessageIOGateway` class converts any `Message` object into a byte-stream using MUSCLE's own binary protocol.
* The default implementation the `CreateGateway()` method of the session classes (`AbstractReflectSession` and its subclasses) creates a `MessageIOGateway` object.
* The binary protocol is based on the output of the `Message::Flatten()` method, with a couple of additional per-Message header-fields for framing.
* `MessageIOGateway` is primarily used in conjunction with TCP connections, but will work over UDP as well (if the flattened-data representation of the `Message` objects will fit inside a UDP packet -- for a more comprehensive Messages-over-UDP implementation, see the [PacketTunnelIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketTunnelIOGateway.html) class)
* `MessageIOGateway` can optionally ZLib-compress its outgoing data stream, to reduce network bandwidth (at the cost of higher CPU usage).  (This compression will be transparent to user of the receiving `MessageIOGateway`)  (See `Message::SetOutgoingEncoding()`)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/iogateway` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [iogateway/example_1_message_to_file.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_1_message_to_file.cpp)
* [iogateway/example_2_message_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_2_message_to_tcp.cpp)
