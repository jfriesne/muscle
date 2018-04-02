# muscle::AbstractMessageIOGateway class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractMessageIOGateway.html)

```#include "iogateway/AbstractMessageIOGateway.h"```

* A gateway is a go-between/mid-level object with two jobs.
* The first job is to receive a stream of incoming bytes from its `DataIO` object and use them to create `Message` objects to pass up to its owner.
* The second job is to receive a series of `Message` objects from its owner and convert them into a stream of outgoing bytes to pass down to its `DataIO` object.
* Gateways are typically used in conjunction with `AbstractReflectSession` objects in a `ReflectServer`.  They handle the session object's communication with its client device.
* The specific rules of how to convert bytes to `Message` objects (and vice-versa) are left up to the subclass.
* The most common subclass to use is the `MessageIOGateway`, which flattens and transmits arbitrary Message objects using MUSCLE's own binary-flattened-Message protocol.
* `PlainTextMessageIOGateway` is a more specialized gateway that converts plain ASCII text into `Message` objects that contain that text in a string field (and vice versa), encoding one line of text into each String value.
* `RawDataMessageIOGateway` simply shovels the incoming raw bytes into a raw-data field of a `Message` object, for the high-level user to deal with verbatim.  No message-framing is attempted.
* `SLIPFramedDataMessageIOGateway` is similar to `RawDataMessageIOGateway`, but it uses the framing rules of the SLIP protocol to partition the incoming byte-stream into appropriate-sized chunks.
* etc

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/iogateway` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [iogateway/example_1_message_to_file.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_1_message_to_file.cpp)
* [iogateway/example_2_message_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_2_message_to_tcp.cpp)
* [iogateway/example_3_text_to_file.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_3_text_to_file.cpp)
* [iogateway/example_4_text_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_4_text_to_tcp.cpp)
