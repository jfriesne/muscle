# MicroMessageGateway C class [(API)](https://public.msli.com/lcs/muscle/html/group__micromessagegateway.html)

```#include "lang/c/micromessage/MicroMessageGateway.h"```

* C-only implementation of the MUSCLE [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html) class
* Handles sending and receiving of flattened-Message-data over a TCP socket (or other transport layer, specified via function pointers)
* Never uses the heap
* Never converts data from 'raw flattened bytes' to a `MicroMessageGateway` object or back

Try compiling and running the \*micro\*.c programs in `muscle/test` (enter `make` to compile everything in the test folder, then run ./microchatclient, and/or ./microreflectclient from the command line)

Quick links to source code of relevant test programs:

* [muscle/tools/microchatclient.c](https://public.msli.com/lcs/muscle/muscle/tools/microchatclient.c)
* [muscle/tools/microreflectclient.c](https://public.msli.com/lcs/muscle/muscle/tools/microreflectclient.c)
