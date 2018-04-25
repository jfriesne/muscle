# muscle::SharedMemory class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SharedMemory.html)

```#include "system/SharedMemory.h"```

An OS-neutral API for creating and accessing a shared-memory region.

* Used to share memory across multiple processes
* The shared memory region is accessed by an agreed-upon globally-unique name, which is passed in to the [SetArea()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SharedMemory.html#a988a40850376df3abfb7f0f313e6e1a1) method by each participating process.
* A [SharedMemory](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SharedMemory.html) object also includes a cross-process reader/writer lock to help synchronize access to the shared data.
* Multiple processes may lock the shared memory area for reading simultaneously (via [LockAreaReadOnly()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SharedMemory.html#a29a7386f2268ac27def0bb1e563aad8a))
* Only one process may lock the shared memory area for writing at a time (via [LockAreaReadWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SharedMemory.html#a6db9a3878451ecfac2559090012f1855))
* [LockAreaReadWrite()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SharedMemory.html#a6db9a3878451ecfac2559090012f1855) won't return until all other locks (both readers' and writers') have been released.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/sharedmemory` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [sharedmemory/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/sharedmemory/example_1_basic_usage.cpp)
