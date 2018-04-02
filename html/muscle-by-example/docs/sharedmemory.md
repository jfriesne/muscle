# muscle::SharedMemory class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SharedMemory.html)

```#include "system/SharedMemory.h"```

* API to create or access a shared memory region
* Use to share memory across multiple processes
* A `SharedMemory` object also includes a cross-process reader/writer lock to help synchronize access to the shared data.
* Multiple processes may lock the shared memory area for reading simultaneously (via `LockAreaReadOnly()`)
* Only one process may lock the shared memory area for writing at a time (via `LockAreaReadWrite()`)
* `LockAreaReadWrite()` won't return until all other locks (reader's or writer's) have been released.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/sharedmemory` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [sharedmemory/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/sharedmemory/example_1_basic_usage.cpp)
