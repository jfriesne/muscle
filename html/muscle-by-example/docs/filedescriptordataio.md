# muscle::FileDescriptorDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDescriptorDataIO.html)

```#include "dataio/FileDescriptorDataIO.h"```

[FileDescriptorDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDescriptorDataIO.html) is used to read/write data from a Unix low-level file-descriptor-handle (`int`, as returned by `open()`, etc).

* The [FileDescriptorDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDescriptorDataIO.html) object will assume ownership of the `int` handle you pass in to it, so don't need to (and shouldn't) call `close()` on it yourself.
* [FileDescriptorDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDescriptorDataIO.html) class can't be used under Windows, since Windows doesn't have this sort of low-level file-handles.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
