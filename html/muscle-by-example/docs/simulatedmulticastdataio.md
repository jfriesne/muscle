# muscle::SimulatedMulticastDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SimulatedMulticastDataIO.html)

```#include "dataio/SimulatedMulticastDataIO.h"```

* Similar to `UDPSocketDataIO`, but written specifically to simulate multicast-UDP traffic over Wi-Fi.
* Transmitting any non-trivial amount of real multicast traffic over Wi-Fi is problematic, because Wi-Fi's multicast support sucks (it's really slow, lossy, and inefficient)
* `SimulatedMulticastDataIO` simulates multicast transmission by automatically maintaining a list of which other `SimulatedMulticastDataIO`-based clients are on the LAN (and using the same multicast address).
* When a client wants to "multicast" a packet, the packet is instead sent multiple times via unicast (once for each other client on the list)
* To the calling code, this all looks the same as real multicast, but works better over Wi-Fi (it will generally be less efficient than actual multicast on a wired network though).
* See also `NetworkInterfaceInfo::GetHardwareType()` (and in particular, when that method returns `NETWORK_INTERFACE_HARDWARE_TYPE_WIFI` you might want to use this class instead of `UDPSocketDataIO`)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
