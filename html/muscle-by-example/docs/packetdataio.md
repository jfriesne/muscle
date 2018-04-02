# muscle::PacketDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html)

```#include "dataio/PacketDataIO.h"```

* `PacketDataIO` is an Abstract Interface that extends the `DataIO` interface.
* A `PacketDataIO` can do the usual `Read()`, `Write()`, etc, but also has the concept of packet-source and packet-destination addresses.
* This sub-interface adds `WriteTo()` and `ReadFrom()` methods to the `DataIO` API.
* This sub-interface also adds `GetMaximumPacketSize()`, `GetSourceOfLastReadPacket()`, and `SetPacketSendDestination()` methods to the `DataIO` API.
* Subclasses of `PacketDataIO` include [UDPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html), and [SimulatedMulticastDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SimulatedMulticastDataIO.html).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_5_packet_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_5_packet_dataio.cpp)
