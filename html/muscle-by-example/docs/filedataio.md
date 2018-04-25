# muscle::FileDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDataIO.html)

```#include "dataio/FileDataIO.h"```

[FileDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDataIO.html) is used to read/write data from a standard C file-handle (`FILE *`, as returned by `fopen()`).

* The [FileDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDataIO.html) object will assume ownership of the `FILE *` handle you pass in to it, so don't need to (and shouldn't) call `fclose()` on it yourself.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_3_seekable_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_3_seekable_dataio.cpp)
* [dataio/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_4_idioms.cpp)
