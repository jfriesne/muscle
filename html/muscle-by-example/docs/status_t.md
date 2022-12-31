# muscle::status_t class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1status__t.html)

```#include "support/MuscleSupport.h"```

Represents whether a function call succeeded or failed (and if it failed, why it failed).

* Extremely lightweight (takes up just one pointer's worth of memory)
* [IsOK()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1status__t.html#ad2f6a88c5da9c1b3233da856b26879ba) method returns true iff the function call succeeded.
* Alternatively, the [IsError()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1status__t.html#a8d176c7fa31ced1dfa7d800b125255c4) method returns true iff the function call failed.
* The [GetDescription()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1status__t.html#aa2e8e791a13ac8923ddb8bc3d762a3d3) method returns a `(const char *)` human-readable string describing the failure (or "OK" if there was no failure)
* Idiom:  [() operator](https://public.msli.com/lcs/muscle/html/classmuscle_1_1status__t.html#a82cbbeb1b0c73c1aeeb861154635c8f4) is a shorthand for calling [GetDescription()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1status__t.html#aa2e8e791a13ac8923ddb8bc3d762a3d3) method to get a `(const char *)`:

```
    const status_t ret = B_IO_ERROR;
    printf("Result was: [%s]\n", ret());  // prints "Result was: [I/O Error]"
```
* Various commonly-used error codes are declared in [MuscleSupport.h](https://public.msli.com/lcs/muscle/html/MuscleSupport_8h.html):
    - `B_NO_ERROR`      - represents a successful operation
    - `B_ERROR`         - a generic error message
    - `B_ERRNO`         - an error message based on the current state of the POSIX `errno` value
    - `B_BAD_ARGUMENT`  - caller passed in an illegal argument
    - `B_OUT_OF_MEMORY` - couldn't allocate memory needed to function
    - `B_IO_ERROR`      - an I/O operation failed
    - etc
* Or you can return your own statically-allocated error-strings, e.g. `return B_ERROR("It's all gone pear-shaped!");`
* There is also an [io_status_t](https://public.msli.com/lcs/muscle/html/classmuscle_1_1io__status__t.html) class which is similar, but also contains an `int32` indicating how many bytes the I/O function processed.
