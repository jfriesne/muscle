# muscle::NullDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html)

```#include "dataio/NullDataIO.h"```

[NullDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html) is a no-op/dummy [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) object that implements write-only memory.

* Any data passed to [NullDataIO::Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html#a0672add49f6c0f410c361f66a9b00ced) will be accepted and discarded.
* [NullDataIO::Read()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html#a044c5d6a5890e2e847b8ddc12f02ea48) will always return 0 (unless [Shutdown()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html#a01d407f5a35c8056c7aaedb4fe173b2e) has been called, in which case it will return `B_END_OF_STREAM`)
* Useful for testing, or when you just don't care what happens to the data your gateway is generating.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
