# muscle::SocketMultiplexer class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html)

```#include "util/SocketMultiplexer.h"```

* Thin abstraction layer around `select()`/`poll()`/`epoll()`/`kqueue()` etc.
* Similar to:  `select()`, `poll()`, `epoll()`, `WaitForMultipleObjects()`
* Default implementation calls through to `select()`, but alternate back-end implementations can be specified on the compile line via e.g. `-DMUSCLE_USE_POLL`, etc.
* On each iteration of your (low-level) event loop, you call `RegisterSocketFor*()` on the `SocketMultiplexer` to tell it which sockets to monitor, then `WaitForEvents()` to block until some I/O is ready to handle.
* After `WaitForEvents()` returns, call `IsSocketReadyFor*()` to find out which sockets are ready to do what.
* This allows you to have a 100%-event-driven program with no wasteful polling.
* `WaitForEvents()` also takes an optional wakeup-time.  If specified, `WaitForEvents()` will return at that time even if no I/O events have been recorded.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/socketmultiplexer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [socketmultiplexer/example_1_tcp_echo_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/socketmultiplexer/example_1_tcp_echo_server.cpp)
* [socketmultiplexer/example_2_tcp_echo_server_with_timed_counter.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/socketmultiplexer/example_2_tcp_echo_server_with_timed_counter.cpp)
* [dataio/example_2_tcp_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_2_tcp_server.cpp)
* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
* [networkutilityfunctions/example_3_udp_multicast.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_3_udp_multicast.cpp)
* [messagetransceiverthread/example_1_threaded_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/messagetransceiverthread/example_1_threaded_smart_client.cpp)
* [iogateway/example_2_message_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_2_message_to_tcp.cpp)
* [iogateway/example_4_text_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_4_text_to_tcp.cpp)
