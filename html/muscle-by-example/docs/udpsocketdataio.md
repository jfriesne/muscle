# muscle::UDPSocketDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html)

```#include "dataio/UDPSocketDataIO.h"```

[UDPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html) handles transmitting and receiving data via UDP packets.

* [UDPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html) is a subclass of [PacketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html), so it has the additional packet-I/O related functionality described in that class.
* [SetPacketSendDestination(const IPAddressAndPort &)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html#aa20de1f9a85cd97588a0454b3b12479a) can be called to specify where [PacketDataIO::Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#a918476c9b709cc5587054c978e6ee721) should send its outgoing UDP packets should be sent to
* [SetPacketSendDestinations(const Queue&lt;IPAddressAndPort&gt; &)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html#a95251531cec38323c36d6b3c4fa2505c) can be called to specify that [PacketDataIO::Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#a918476c9b709cc5587054c978e6ee721) should send each outgoing UDP packet to multiple destinations.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_5_packet_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_5_packet_dataio.cpp)
* [reflector/example_7_smart_server_with_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_7_smart_server_with_udp_pingpong.cpp)
