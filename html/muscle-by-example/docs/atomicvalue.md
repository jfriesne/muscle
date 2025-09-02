# muscle::AtomicValue class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicValue.html)

```#include "system/AtomicValue.h"```

Holds an internal lockless/thread-safe ring-buffer containing multiple versions of a particular C++ object, so that writer-threads can call [SetValue()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicValue.html#a5480fdd72ee1d8355e60bd1c56f51359) to update the object without causing "object tearing" problems for reader-threads that might be in the middle of reading from an existing version of the object.

* Used primarily in situations where multiple threads want to share access to a variable but don't want to synchronize access to it using a [Mutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html) (e.g. because some of the threads are running in a real-time context where locking a [Mutex](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Mutex.html) isn't allowed)
* [GetValue()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicValue.html#a2e4bb88ca2c1728506e8a1024568390a) copies out the most recent valid version of the object from the ring-buffer.
* [GetValueRef()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicValue.html#ab80414594e54a52bd5fa137652fa1568) returns a reference to the most recent valid version of the object in the ring-buffer (note that the reference may not remain valid indefinitely, so use this method with caution)
* [SetValue()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicValue.html#a5480fdd72ee1d8355e60bd1c56f51359) increments the internal ring-buffer's write-index, and copies the passed-in value into the next available slot.
* Note that calling [SetValue()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicValue.html#a5480fdd72ee1d8355e60bd1c56f51359) too often may result in the call erroring out (returning B_RESOURCE_LIMIT) due to ring-buffer contention; this class is best used for variables that only get updated infrequently.

Try compiling and running the mini-example-program in `muscle/html/muscle-by-example/examples/atomicvalue` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example program:

* [atomiccounter/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/atomicvalue/example_1_basic_usage.cpp)
