# Network Utility Functions [(API)](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html)

```#include "util/NetworkUtilityFunctions.h"```

* User-friendly C++ wrapper around the [BSD Sockets C API](https://en.wikipedia.org/wiki/Berkeley_sockets)
* Uses `ConstSocketRef` (instead of `int`) to represent file descriptors, to remove any chance of file descriptor "leaks"
* Code written to this API will "just work" on both IPv4 and IPv6 (via dual-stack mode)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/networkutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [networkutilityfunctions/example_1_tcp_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_1_tcp_client.cpp)
* [networkutilityfunctions/example_2_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_2_udp_pingpong.cpp)
* [networkutilityfunctions/example_3_udp_multicast.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_3_udp_multicast.cpp)
