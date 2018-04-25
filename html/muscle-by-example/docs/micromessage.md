# MicroMessage C class [(API)](https://public.msli.com/lcs/muscle/html/group__micromessage.html)

```#include "micromessage/MicroMessage.h"```

* C-only implementation of the MUSCLE [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) class
* Flattened-data format is 100% compatible with the C++ implementation (i.e. can be used to communicate with C++ based MUSCLE programs)
* Never uses the heap
* Never converts data from 'raw flattened bytes' to a `MicroMessage` object or back
* Inspecting a `MicroMessage` is done by directly reading the 'raw flattened bytes' from a byte-buffer.
* Constructing a `MicroMessage` is equivalent to directly writing the 'raw flattened bytes' into a byte-buffer.
* The only modification allowed to a `MicroMessage` is appending more data to it (random-access editing isn't supported)
* Useful for platforms that don't have a C++ compiler (or whose C++ compiler is too antiquated to compile the MUSCLE C++ codebase)
* Useful for very constrained (e.g. MMU-less) environments where you want to avoid heap fragmentation.

Try compiling and running the \*micro\*.c programs in `muscle/test` (enter `make` to compile everything in the test folder, then run ./microchatclient and/or ./microreflectclient from the command line)

Quick links to source code of relevant MUSCLE-by-example programs:

* [muscle/test/microchatclient.c](https://public.msli.com/lcs/muscle/muscle/test/microchatclient.c)
* [muscle/test/microreflectclient.c](https://public.msli.com/lcs/muscle/muscle/test/microreflectclient.c)
