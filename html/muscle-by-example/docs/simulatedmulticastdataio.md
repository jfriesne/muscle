# muscle::SimulatedMulticastDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SimulatedMulticastDataIO.html)

```#include "dataio/SimulatedMulticastDataIO.h"```

[SimulatedMulticastDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SimulatedMulticastDataIO.html) appears to work a lot like a [UDPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html), but it is written specifically to simulate multicast-UDP traffic over Wi-Fi using directed unicast.

* Transmitting any non-trivial amount of real multicast traffic over Wi-Fi is problematic, because Wi-Fi's multicast support sucks (it's really slow, lossy, and inefficient)
* [SimulatedMulticastDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SimulatedMulticastDataIO.html) simulates multicast transmission by automatically maintaining a list of which other [SimulatedMulticastDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SimulatedMulticastDataIO.html)-based clients are on the LAN (and using the same multicast address).
* When a client wants to "multicast" a packet, the packet is instead sent multiple times via unicast (once for each other client on the list)
* To the calling code, this all looks the same as real multicast, but works better over Wi-Fi (it will generally be less efficient than actual multicast on a wired network though).
* See also [NetworkInterfaceInfo::GetHardwareType()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NetworkInterfaceInfo.html#a4c7ba080591bc21b63ce19493e0ae408) (and in particular, when that method returns [NETWORK_INTERFACE_HARDWARE_TYPE_WIFI](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a2970898e8a43ce21e1cc510d49f1b89dac2d118d070e021ddc0d8ed43fda53971) you might want to use this class instead of [UDPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UDPSocketDataIO.html))

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
