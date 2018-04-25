# muscle::NullDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html)

```#include "dataio/NullDataIO.h"```

[NullDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html) is a no-op/dummy [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) object that implements write-only memory.

* Any data passed to [NullDataIO::Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html#a1678b51bff36c393e3b279a73b972366) will be accepted and discarded.
* [NullDataIO::Read()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html#ab13ddbab8904694a9b7f621ee0b4fc78) will always return 0 (unless [Shutdown()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html#a01d407f5a35c8056c7aaedb4fe173b2e) has been called, in which case it will return -1/error)
* Useful for testing, or when you just don't care what happens to the data your gateway is generating.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
