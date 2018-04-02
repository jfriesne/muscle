# muscle::Socket class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Socket.html)

```#include "util/Socket.h"```

* Holds/owns an `int` socket/file descriptor
* Similar to:  `QTcpSocket`, `QUdpSocket`
* `Socket`'s destructor calls `close()` on the file descriptor (`closesocket()` on Windows)
* Not typically used directly; usually passed around via `ConstSocketRef` instead
* NetworkUtilityFunctions.h functions return a `ConstSocketRef` object and/or take one as an argument.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/socket` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [socket/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/socket/example_1_basic_usage.cpp)
* [networkutilityfunctions/example_1_tcp_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_1_tcp_client.cpp)
* [networkutilityfunctions/example_2_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_2_udp_pingpong.cpp)
* [networkutilityfunctions/example_3_udp_multicast.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_3_udp_multicast.cpp)
