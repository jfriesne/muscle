# muscle::Flattenable class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html)

```#include "support/Flattenable.h"```

* Abstract interface representing an object that knows how to "flatten" itself (by writing its current state into a raw byte-array), and "unflatten" itself (by reading in a new state for itself from a similar raw byte-array)
* Used to facilitate unstructured/low-level I/O and also for adding user-defined data types to a `Message` object.
* `Flattenable::TypeCode()` returns a 32-bit type-code used to identify the object's type (useful for type-checking)
* `PseudoFlattenable` is an empty/no-op interface, used the same as `Flattenable`, but it declares no virtual methods -- classes inherit from `PseudoFlattenable` only to mark themselves as Flattenable "in spirit", and can be used by templated methods like `Message::AddFlat()` without having to pay the extra-pointer-per-object tax that comes with using virtual methods.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/flattenable` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [flattenable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/flattenable/example_1_basic_usage.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
