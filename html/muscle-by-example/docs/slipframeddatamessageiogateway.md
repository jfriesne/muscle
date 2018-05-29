# muscle::SLIPFramedDataMessageIOGateway class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SLIPFramedDataMessageIOGateway.html)

```#include "iogateway/SLIPFramedDataMessageIOGateway.h"```

The [SLIPFramedDataMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SLIPFramedDataMessageIOGateway.html) class transmits raw binary data across the network, but frames it using the SLIP framing protocol (RFC 1055).

* Useful for getting your raw binary data across in well-defined chunks (assuming the program on the other end of the connection is also using a [SLIPFramedDataMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SLIPFramedDataMessageIOGateway.html) or some other SLIP implementation)
* Note that (unlike [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html), but similar to [RawDataMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RawDataMessageIOGateway.html)) this gateway won't losslessly transmit any arbitrary [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object; rather it only uses Messages as a carrier for binary data.  Any other field names or data types besides `PR_NAME_DATA_CHUNKS`/`B_RAW_TYPE` will be ignored.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/iogateway` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
