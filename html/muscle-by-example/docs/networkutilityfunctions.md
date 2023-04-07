# Network Utility Functions [(API)](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html)

```#include "util/NetworkUtilityFunctions.h"```

MUSCLE's user-friendly C++ wrapper around the [BSD Sockets C API](https://en.wikipedia.org/wiki/Berkeley_sockets)

* Uses [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html) (instead of `int`) to represent file descriptors, to avoid any chance of file descriptor "leaks"
* Code written to this API will generally "just work" over both IPv4 and IPv6, no special-case code required.
* [GetHostByName()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga9c65f71b0188d2f3109c733d34fc8b6c) returns the [IPAddress](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddress.html) associated with a given hostname.
* [CreateUDPSocket()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga2c5486f4c87b84d1eddb3316a308ac7a) creates a UDP socket.
* [BindUDPSocket()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#gaa72e28165038ba6fd63c2954748b5bdc) binds the UDP socket to a local port.
* [Connect()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#gae0b2161231db21576ac4d87a943dac85) returns a TCP socket that is connected to the specified host and port.
* [CreateAcceptingSocket()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga671b4f8b05dd461df566f6139021fe78) returns a socket that can be used to accept TCP connections.
* [Accept()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#gafef53af40a9f71726865ff3de1f19ae3) accepts an incoming TCP connection and returns a [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html) representing the new connection.
* [SendData()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga8bb299dc6036189b576b9634a0e52899) sends bytes over a TCP connection.
* [SendDataUDP()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga254bb8b4337e954fe14b642aef364fc7) sends bytes out via a UDP packet.
* [ReceiveData()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga8bab842568b6d231e491a97190aed572) receives bytes over a TCP connection.
* [ReceiveDataUDP()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga25aecc954885c328e9118d955036b777) receives bytes from an incoming UDP packet.
* ... etc etc.
* Note that there is no `CloseSocket()` function, because it's not necessary.  Sockets will automatically close themselves when their last [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html) goes away, via RAII-magic.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/networkutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [networkutilityfunctions/example_1_tcp_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_1_tcp_client.cpp)
* [networkutilityfunctions/example_2_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_2_udp_pingpong.cpp)
* [networkutilityfunctions/example_3_udp_multicast.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_3_udp_multicast.cpp)
