# muscle::DetectNetworkConfigChangesSession class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DetectNetworkConfigChangesSession.html)

```#include "system/DetectNetworkConfigChangesSession.h"```

An OS-neutral mechanism for detecting (and reacting to) changes to the host computer's networking configuration.

* This session can be added to your [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html) if you want to react to changes in the computer's network config (e.g. Ethernet interfaces going up or down, user changing DHCP settings, IP addresses changing, etc)
* When a session of this type is present in your [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html), any other sessions that subclass from [INetworkConfigChangesTarget](https://public.msli.com/lcs/muscle/html/classmuscle_1_1INetworkConfigChangesTarget.html) will have their callback methods called at the appropriate times.
* As a bonus, this session can also tell you when the computer is [about to go to sleep](https://public.msli.com/lcs/muscle/html/classmuscle_1_1INetworkConfigChangesTarget.html#a2c6727307a52f5b06d67b4a644fd570b), or [just woke up](https://public.msli.com/lcs/muscle/html/classmuscle_1_1INetworkConfigChangesTarget.html#aca77828cd3f199719a9763cfcc8df356)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/netconfig` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [netconfig/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/netconfig/example_1_basic_usage.cpp)
* [netconfig/example_2_without_subclassing.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/netconfig/example_2_without_subclassing.cpp)
