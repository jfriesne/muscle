# muscle::ThreadLocalStorage class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ThreadLocalStorage.html)

```#include "system/ThreadLocalStorage.h"```

* `ThreadLocalStorage` is a templated class that lets a (typically) global/static variable keep a different state for each thread that accesses it.
* Declare the ThreadLocalStorage<T> object e.g. as a static variable
* Each thread can call `GetOrCreateThreadLocalObject()` on the `ThreadLocalStorage` object to get a pointer to the underlying per-thread variable, and then use it the way they want to (without worrying about other threads seeing there version of that value)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/threadlocalstorage` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [threadlocalstorage/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/threadlocalstorage/example_1_basic_usage.cpp)
