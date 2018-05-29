# muscle::Flattenable class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html)

```#include "support/Flattenable.h"```

Abstract interface representing an object that knows how to [Flatten()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html#a2dc675514469c6d25931e45866773691) itself (by writing its current state into a raw-byte-array, via some well-defined encoding), and [Unflatten()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html#a2b8439db9dcea4ef101f8f38ab668f74) itself (by reading in a new state for itself from a previously-flattened-to raw-byte-array)

* Used for low-level object-persistence and networking, and also for adding user-defined data types to a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object.
* Similar to:  [BFlattenable](https://www.haiku-os.org/docs/api/classBFlattenable.html)
* [Flattenable::TypeCode()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html#a9956854e3c3c2c8a6ed1aae9fbc49274) returns a 32-bit type-code used to identify the object's type (useful for quick sanity-checking)
* [Flattenable::FlattenedSize()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html#a81e896d19a0833810f75e7cd4cf0570f) must return how many bytes [Flatten()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html#a2dc675514469c6d25931e45866773691) would write out if called.
* [PseudoFlattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PseudoFlattenable.html) is an empty/no-op interface class, used in the same way as [Flattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html), but it declares no virtual methods.  Lightweight classes can inherit from [PseudoFlattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PseudoFlattenable.html) merely to self-document as [Flattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html) "in spirit", and can be used by templated methods (like [Message::AddFlat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#a9d840110ff9313a3c7d90de9eb3b680a) and [Message::FindFlat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#a38b2ae385faee315af284f8f8da379d6)) without having to pay the extra-pointer-per-object tax that comes with using virtual methods.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/flattenable` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [flattenable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/flattenable/example_1_basic_usage.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
