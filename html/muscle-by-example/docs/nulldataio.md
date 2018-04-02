# muscle::NullDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html)

```#include "dataio/NullDataIO.h"```

* `NullDataIO` is a no-op/dummy `DataIO` object that implements write-only memory.
* Any data passed to `NullDataIO::Write()` will be accepted and immediately thrown away.
* `NullDataIO::Read()` will always return 0 (unless `Shutdown()` has been called, in which case it will return -1/error)
* Useful for testing, or when you just don't care what happens to the data your gateway is generating.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
