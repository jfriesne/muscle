# muscle::SignalMessageIOGateway class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SignalMessageIOGateway.html)

```#include "iogateway/SignalMessageIOGateway.h"```

[SignalMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SignalHandlerSession.html) is a "dummy gateway" used solely for thread-to-thread signalling purposes.

* This gateway isn't really used to transmit data; instead it is only used to send "nudges"
* Whenever one or more [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) objects are sent to this gateway by its owner, this gateway will push a single 'notification byte' to its [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) object.
* Whenever one or more bytes are received from the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) object, this gateway will pass a single 'notification [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html)' up to its owning session.
* This gateway is used primarily for inter-thread communication (specifically, the internal thread of a [Thread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Thread.html) object sends a notification-message to the master thread telling it to go ahead and [check its reply-Queue](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Thread.html#a44f2b8d04d2c7fb0e6a7995440cfaab8) for new [Messages](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html), and [vice-versa](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Thread.html#ab165fef7c04d2d15bdce9069eff32bc5)).  The actual inter-thread [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) data itself is never sent through the gateway, since [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) serialization/deserialization is an unnecessary and avoidable expense when communicating between two threads in the same process.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/iogateway` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [thread/example_2_dumb_server_with_thread.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/thread/example_2_dumb_server_with_thread.cpp)
