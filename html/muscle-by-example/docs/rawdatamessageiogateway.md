# muscle::RawDataMessageIOGateway class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RawDataMessageIOGateway.html)

```#include "iogateway/RawDataMessageIOGateway.h"```

The [RawDataMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RawDataMessageIOGateway.html) class transmits raw binary data across the network.

* This class is useful if you don't want to attempt any data-parsing at the gateway level, but just want the raw bytes delivered verbatim to your higher-level code.
* Incoming bytes of binary data added to Message objects via `msg.AddData(PR_NAME_DATA_CHUNKS, B_RAW_TYPE, pointerToBytes, numBytes)`, and these Messages are passed up to the gateway's user.
* No framing logic is attempted, so the sizes of the "chunks" of raw data will be largely indeterminate (although you can specify a minimum and maximum chunk-size in the [RawDataMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RawDataMessageIOGateway.html) constructor, if you want to)
* Outgoing Message objects are examined, and if they contain a B_RAW_TYPE field named `PR_NAME_DATA_CHUNKS` (aka "rd"), those bytes will be sent across the network.
* [CountedRawDataMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1CountedRawDataMessageIOGateway.html) is the same as [RawDataMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RawDataMessageIOGateway.html) except that it keeps a running tally of the number of bytes currently present in its outgoing-Messages-queue.  This can be handy if you want to monitor/limit the memory footprint of that queue.
* Note that (unlike [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html)) this gateway won't losslessly transmit any arbitrary [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object; rather it only uses Messages as a carrier for binary data.  Any other field names or data types (besides `PR_NAME_DATA_CHUNKS`/`B_RAW_TYPE`) will be ignored.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/iogateway` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [reflector/example_7_smart_server_with_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_7_smart_server_with_udp_pingpong.cpp)
