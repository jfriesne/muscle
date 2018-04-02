# muscle::SignalMessageIOGateway class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SignalMessageIOGateway.html)

```#include "iogateway/SignalMessageIOGateway.h"```

* This gateway isn't really used to transmit data; instead it is only used to send "nudges"
* If one or more `Message` objects are sent to this gateway by its owner, this gateway will push a single 'notification byte' to its `DataIO` object.
* If one or more bytes are received from the `DataIO` object, this gateway will pass a single 'notification Message' up to its owning session.
* This gateway is used primarily for inter-thread communication (specifically, the internal thread of a `Thread` object sends a notification-message to the master thread telling it to go ahead and check its reply-Queue for new Messages, and vice-versa).  The actual inter-thread Message data itself is never sent through the gateway, since data serialization/deserialization is an avoidable and unnecessary expense when communicating between threads in the same process.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/iogateway` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [thread/example_2_dumb_server_with_thread.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/thread/example_2_dumb_server_with_thread.cpp)
