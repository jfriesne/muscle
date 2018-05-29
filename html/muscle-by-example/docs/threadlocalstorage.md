# muscle::ThreadLocalStorage class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ThreadLocalStorage.html)

```#include "system/ThreadLocalStorage.h"```

[ThreadLocalStorage](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ThreadLocalStorage.html) is a templated class that lets a (typically) global/static variable keep a different state for each thread that accesses it.

* Declare the [ThreadLocalStorage&lt;T&gt;](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ThreadLocalStorage.html) object e.g. as a static variable
* Each thread can call [GetOrCreateThreadLocalObject()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ThreadLocalStorage.html#a7936d3d483ea0e5806ab130a1d38beb6) on the [ThreadLocalStorage](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ThreadLocalStorage.html) object to get a pointer to the underlying per-thread variable, and then use it the way they want to (i.e. without worrying about other threads seeing or modifying their instance of that value)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/threadlocalstorage` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [threadlocalstorage/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/threadlocalstorage/example_1_basic_usage.cpp)
