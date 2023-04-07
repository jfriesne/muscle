# muscle::PacketDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html)

```#include "dataio/PacketDataIO.h"```

[PacketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html) is an interface that extends the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) interface to handle UDP-style packet I/O.

* A [PacketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html) can do the usual [Read()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#af093606c8aab0b42632e89a372a0d4e8), [Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#ada04bde999b32675319dab05e797588c), etc, but also has the concept of packet-source and packet-destination addresses.
* This sub-interface adds [WriteTo()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#afae905ddb3aac538720b94801138af64) and [ReadFrom()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a5bad705976debaca4f9ac0cfd4acfe01) methods to the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) API.
* This sub-interface also adds [GetMaximumPacketSize()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a94d69b7827190cf4bd9439d1d61de256), [GetSourceOfLastReadPacket()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a6af20b00f96a4c14ee7b5a43e692b8ae), and [SetPacketSendDestination()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html#a6aebcecc19a296a507695b9bfb52c5e8) methods to the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) API.
* Subclasses of [PacketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html) include [UDPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html), and [SimulatedMulticastDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SimulatedMulticastDataIO.html).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [dataio/example_5_packet_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_5_packet_dataio.cpp)
