# muscle::SocketMultiplexer class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html)

```#include "util/SocketMultiplexer.h"```

A lightweight abstraction-layer around I/O multiplexing APIs (`select()`/`poll()`/`epoll()`/`kqueue()` etc).

* Similar to:  [select()](http://man7.org/linux/man-pages/man2/select.2.html), [poll()](http://man7.org/linux/man-pages/man2/poll.2.html), [epoll()](http://man7.org/linux/man-pages/man7/epoll.7.html), [WaitForMultipleObjects()](https://msdn.microsoft.com/en-us/library/windows/desktop/ms687025(v=vs.85).aspx)
* Default implementation calls through to `select()`, but an alternate back-end implementation can be specified on the compile line via e.g. `-DMUSCLE_USE_POLL`, etc.
* On each iteration of your (low-level) event loop, call [RegisterSocketForReadReady()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html#a3c9c318a3b6c095fc64707bdf581bf71) and/or [RegisterSocketForWriteReady()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html#a2f8cf73a6b21e47f4ea9176326ce2b93) on the [SocketMultiplexer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html) to tell it which sockets to monitor, then call [WaitForEvents()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html#abda53005ba0f856395838607fa971ccc) to block until some I/O is ready to handle.
* After [WaitForEvents()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html#abda53005ba0f856395838607fa971ccc) returns, call [IsSocketReadyForRead()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html#a61e434ef659eb84ef83cac4fba7415c1) and/or [IsSocketReadyForWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html#ab9b757c4526c2ead0dce9efd52d485e1) to find out which sockets are ready to do what category of I/O.
* This allows you to have a 100%-event-driven program with no wasteful polling.
* [WaitForEvents()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html#abda53005ba0f856395838607fa971ccc) also takes an optional wakeup-time.  If specified, [WaitForEvents()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html#abda53005ba0f856395838607fa971ccc) will return at that time even if no I/O events have been recorded.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/socketmultiplexer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [socketmultiplexer/example_1_tcp_echo_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/socketmultiplexer/example_1_tcp_echo_server.cpp)
* [socketmultiplexer/example_2_tcp_echo_server_with_timed_counter.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/socketmultiplexer/example_2_tcp_echo_server_with_timed_counter.cpp)
* [dataio/example_2_tcp_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_2_tcp_server.cpp)
* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
* [networkutilityfunctions/example_3_udp_multicast.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/networkutilityfunctions/example_3_udp_multicast.cpp)
* [messagetransceiverthread/example_1_threaded_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/messagetransceiverthread/example_1_threaded_smart_client.cpp)
* [iogateway/example_2_message_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_2_message_to_tcp.cpp)
* [iogateway/example_4_text_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_4_text_to_tcp.cpp)
