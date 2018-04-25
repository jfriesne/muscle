# muscle::MessageIOGateway class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html)

```#include "iogateway/MessageIOGateway.h"```

The [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html) class converts any [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object into a byte-stream (and vice-versa) using MUSCLE's own binary protocol.

* The default implementation of the [CreateGateway()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractReflectSession.html#a9c73fb899e0c122d925751342568ef13) method of the [session](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractReflectSession.html) classes creates a [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html) object.
* The binary protocol is based on the output of the [Message::Flatten()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#a07febe3214e3380aa10c27cbd8294873) method, with a couple of additional per-Message header-fields for framing.
* [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html) is primarily used in conjunction with TCP connections, but will work over UDP as well (if the flattened-data representation of the [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) objects will fit inside a UDP packet -- for a more comprehensive Messages-over-UDP implementation, see the [PacketTunnelIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketTunnelIOGateway.html) class)
* [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html) can optionally ZLib-compress its outgoing data stream, to reduce network bandwidth (at the cost of higher CPU usage).  (This compression will be transparent to user of the receiving [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html))  (See [MessageIOGateway::SetOutgoingEncoding()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html#acdc4d282f8fe6494bd2ec31e36bb7822))

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/iogateway` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [iogateway/example_1_message_to_file.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_1_message_to_file.cpp)
* [iogateway/example_2_message_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_2_message_to_tcp.cpp)
