# muscle::PacketDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html)

```#include "dataio/PacketDataIO.h"```

[PacketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html) is an interface that extends the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) interface to handle UDP-style packet I/O.

* A [PacketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html) can do the usual [Read()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#a4793574f952157131382b248886b136f), [Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#a918476c9b709cc5587054c978e6ee721), etc, but also has the concept of packet-source and packet-destination addresses.
* This sub-interface adds [WriteTo()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a9de4f8e072d158d56b0db256b0a53b6b) and [ReadFrom()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a967782889209ef8963bb3df16b65cbf3) methods to the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) API.
* This sub-interface also adds [GetMaximumPacketSize()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a94d69b7827190cf4bd9439d1d61de256), [GetSourceOfLastReadPacket()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a2235a8f41dc0de6639420958b3d62563), and [SetPacketSendDestination()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a6aebcecc19a296a507695b9bfb52c5e8) methods to the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) API.
* Subclasses of [PacketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html) include [UDPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html), and [SimulatedMulticastDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SimulatedMulticastDataIO.html).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_5_packet_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_5_packet_dataio.cpp)
