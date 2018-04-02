# muscle::UDPSocketDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html)

```#include "dataio/UDPSocketDataIO.h"```

* `UDPSocketDataIO` is used to handle transmitting and receiving data over a UDP socket.
* `UDPSocketDataIO` is a subclass of `PacketDataIO`, so it has the additional packet-I/O related functionality described in that class.
* `SetPacketSendDestination(const IPAddressAndPort &)` can be called to specify where outgoing UDP packets should be sent to
* `SetPacketSendDestinations(const Queue<IPAddressAndPort> &)` can be called to specify that each outgoing UDP packet should be sent to multiple destinations.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_5_packet_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_5_packet_dataio.cpp)
* [reflector/example_7_smart_server_with_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_7_smart_server_with_udp_pingpong.cpp)
