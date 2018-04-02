# muscle::Message class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html)

```#include "message/Message.h"```

* Primary unit of network and inter-thread communication in MUSCLE
* Similar to: `BMessage`, `JSON Message (?)`, `XML (?)`
* Can holds any number of data-fields (up to 2^32 fields)
* Each data field has a UTF-8 name (which is used to uniquely identify it) and 1 or more data-values of its type
* Supported field-data-types include:  `int8`, `int16`, `int32`, `int64`, `float`, `double`, `String`, `Message`, `Point`, `Rect`, plus any arbitrary additional types the user cares to define.
* Each `Message` also includes a single `uint32` "what-code", which is typically used for tagging and quick dispatch (e.g. via a `switch` statement)
* A `Message` is `Flattenable`:  i.e. serializable to a platform-neutral flattened-bytes representation that can be saved to disk or transmitted across the network, and later parsed unambiguously under any OS, CPU, or language.
* A `Message` is `RefCountable`, which allows efficient (zero-copy) in-process transmission of `Messages` to other threads and functions, and efficient adding of `Message` objects to data structures (like the server-side `DataNode` tree)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/message` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [message/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_1_basic_usage.cpp)
* [message/example_2_nested_messages.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_2_nested_messages.cpp)
* [message/example_3_add_archive.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_3_add_archive.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
* [message/example_5_field_iteration.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_5_field_iteration.cpp)
