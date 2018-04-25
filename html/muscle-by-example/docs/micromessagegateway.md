# MicroMessageGateway C class [(API)](https://public.msli.com/lcs/muscle/html/group__micromessagegateway.html)

```#include "micromessage/MicroMessageGateway.h"```

* C-only implementation of the MUSCLE [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html) class
* Handles sending and receiving of flattened-Message-data over a TCP socket (or other transport layer, specified via function pointers)
* Never uses the heap
* Never converts data from 'raw flattened bytes' to a `MicroMessageGateway` object or back

Try compiling and running the \*micro\*.c programs in `muscle/test` (enter `make` to compile everything in the test folder, then run ./microchatclient, and/or ./microreflectclient from the command line)

Quick links to source code of relevant test programs:

* [muscle/test/microchatclient.c](https://public.msli.com/lcs/muscle/muscle/test/microchatclient.c)
* [muscle/test/microreflectclient.c](https://public.msli.com/lcs/muscle/muscle/test/microreflectclient.c)
