# Network Utility Functions [(API)](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html)

```#include "util/NetworkUtilityFunctions.h"```

MUSCLE's user-friendly C++ wrapper around the [BSD Sockets C API](https://en.wikipedia.org/wiki/Berkeley_sockets)

* Uses [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html) (instead of `int`) to represent file descriptors, to avoid any chance of file descriptor "leaks"
* Code written to this API will generally "just work" over both IPv4 and IPv6, no special-case code required.
* [GetHostByName()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga5faef555511356cd7f58ad65f72b4669) returns the [IPAddress](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddress.html) assocaited with a given hostname.
* [CreateUDPSocket()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga518bcf2c4446cd8ebdacee98cc9e1880) creates a UDP socket.
* [BindUDPSocket()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#gad080462f95926cd17ef349bf6df6ddd4) binds the UDP socket to a local port.
* [Connect()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga7b33d4801f68ff0fdb0e120cfbd448ad) returns a TCP socket that is connected to the specified host and port.
* [CreateAcceptingSocket()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga9e64c54918dc8a49ca396a54cf106510) returns a socket that can be used to accept TCP connections.
* [Accept()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga4432a358162ec504701937831d2329a7) accepts an incoming TCP connection and returns a [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html) representing the new connection.
* [SendData()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#gae5353efd5ff5c11cf9524ca45a87974c) sends bytes over a TCP connection.
* [SendDataUDP()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga5f0a49da4de4d96a7107815009f30986) sends bytes out via a UDP packet.
* [ReceiveData()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#ga3f3bbb49c0eb957740aa02d68a03d164) receives bytes over a TCP connection.
* [ReceiveDataUDP()](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html#gabced40d584b31406e02ea2189d6a06e4) receives bytes from an incoming UDP packet.
* ... etc etc.
* Note that there is no `CloseSocket()` function, because it's not necessary.  Sockets will automatically close themselves when their last [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html) goes away, via RAII-magic.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/networkutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [networkutilityfunctions/example_1_tcp_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_1_tcp_client.cpp)
* [networkutilityfunctions/example_2_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_2_udp_pingpong.cpp)
* [networkutilityfunctions/example_3_udp_multicast.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_3_udp_multicast.cpp)
