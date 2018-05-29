# muscle::Socket class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Socket.html)

```#include "util/Socket.h"```

An RAII-friendly socket/file-descriptor wrapper.

* Holds/owns an `int` socket/file descriptor.
* Similar to:  [QTcpSocket](http://doc.qt.io/qt-5/qtcpsocket.html), [QUdpSocket](http://doc.qt.io/qt-5/qudpsocket.html)
* [Socket](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Socket.html)'s destructor calls `close()` on the file descriptor (`closesocket()` on Windows)
* Not typically used directly; usually passed around via [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html) instead
* [NetworkUtilityFunctions.h](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html) functions often return a [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html) object and/or take one as an argument.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/socket` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [socket/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/socket/example_1_basic_usage.cpp)
* [networkutilityfunctions/example_1_tcp_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_1_tcp_client.cpp)
* [networkutilityfunctions/example_2_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_2_udp_pingpong.cpp)
* [networkutilityfunctions/example_3_udp_multicast.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_3_udp_multicast.cpp)
