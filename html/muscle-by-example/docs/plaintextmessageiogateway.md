# muscle::PlainTextMessageIOGateway class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PlainTextMessageIOGateway.html)

```#include "iogateway/PlainTextMessageIOGateway.h"```

The [PlainTextMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PlainTextMessageIOGateway.html) class communicates over the network via lines of plain old ASCII text.

* Incoming lines of plain text are added to Message objects via `msg.AddString(PR_NAME_TEXT_LINE, "the incoming text goes here")`, and these Messages are passed up to the gateway's user.
* Outgoing Message objects are examined, and if they contain a String field named `PR_NAME_TEXT_LINE` (aka "tl"), that text will be sent across the network.
* Carriage returns and newlines are automatically handled by the gateway.
* Note that (unlike [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html)) this gateway won't losslessly transmit any arbitrary [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object; rather it only uses Messages as a carrier for text lines.  Any other field names or data types (besides `PR_NAME_TEXT_LINE`/`B_STRING_TYPE`) will be ignored.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/iogateway` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [example_3_text_to_file.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_3_text_to_file.cpp)
* [example_4_text_to_tcp.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/iogateway/example_4_text_to_tcp.cpp)
