# muscle::NetworkInterfaceInfo class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NetworkInterfaceInfo.html)

```#include "util/NetworkInterfaceInfo.h"```

Holds discovered information about a local network device (Ethernet card, Wi-Fi, etc)

* Similar to:  [QNetworkInterface](http://doc.qt.io/qt-5/qnetworkinterface.html)
* Contains info like the NIC's name, human-readable description, MAC address, netmask, copper status, hardware type, etc.
* Call [GetNetworkInterfaceInfos()](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a6007b7af420ba81966942422942f580a) (in the [NetworkUtilityFunctions API](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html)) to populate a `Queue<NetworkInterfaceInfo>` with all the info about the host computer's attached network interfaces
* A [DetectNetworkConfigChangesSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DetectNetworkConfigChangesSession.html) object can optionally be used to notify your program whenever the local computer's network configuration has changed, in case it wants to respond to that (e.g. by reconnecting sockets)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/networkinterfaceinfo` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [networkinterfaceinfo/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkinterfaceinfo/example_1_basic_usage.cpp)
