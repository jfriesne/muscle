## MicroMessage bare-bones C API

```
#include "micromessage/MicroMessage.h"
#include "micromessage/MicroMessageGateway.h
```

The `MicroMessage` API is an extremely bare-bones, low-level C 
implementation of the MUSCLE [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) and [MessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageIOGateway.html) APIs.

This implementation never bothers to flatten or unflatten message
objects at all; instead it reads and writes flattened-message-data
directly from/to a raw buffer of bytes.

That makes this implementation very efficient (it never allocates 
or frees heap memory), but it does restrict its functionality.  

In particular when constructing a `MicroMessage` data-buffer, 
data can only be appended.  Data already existing in the
`MicroMessage` buffer is considered read-only.  Also, you can only
add as much data as will fit into the byte-buffer you provided.
Attempts to add more data than that will fail.

The only two files needed to use the `MicroMessage` API are `MicroMessage.c`
and (optionally) `MicroMessageGateway.c`  These files do not depend 
on any other files outside of this folder, except 
for `support/MuscleSupport.h`.

See also the [micromessage README.TXT file](https://public.msli.com/lcs/muscle/muscle/micromessage/README.TXT)
