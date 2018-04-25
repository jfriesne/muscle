# muscle::DetectNetworkConfigChangesSession class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DetectNetworkConfigChangesSession.html)

```#include "system/DetectNetworkConfigChangesSession.h"```

An OS-neutral mechanism for detecting (and reacting to) changes to the host computer's networking configuration.

* This session can be added to your [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html) if you want to react to changes in the computer's network config (e.g. Ethernet interfaces going up or down, user changing DHCP settings, IP addresses changing, etc)
* As a bonus, this session can also tell you when the computer is [about to go to sleep](https://public.msli.com/lcs/muscle/html/classmuscle_1_1INetworkConfigChangesTarget.html#a436d825e7cdb5476a28e9e528e81b524), or [just woke up](https://public.msli.com/lcs/muscle/html/classmuscle_1_1INetworkConfigChangesTarget.html#abc3ed3742eab86ebec9de64526f1fdf1)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/detectnetworkconfigchangessession` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [detectnetworkconfigchangessession/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/detectnetworkconfigchangessession/example_1_basic_usage.cpp)
* [detectnetworkconfigchangessession/example_2_without_subclassing.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/detectnetworkconfigchangessession/example_2_without_subclassing.cpp)
