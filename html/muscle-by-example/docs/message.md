# muscle::Message class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html)

```#include "message/Message.h"```

[Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) is the main unit of network and inter-thread communication in MUSCLE.

* Similar to: [BMessage](http://download.unirc.eu/BeOS/Docs/BeBook/The%20Application%20Kit/Message.html), `JSON Message (?)`, `XML (?)`
* Can hold any number of uniquely-named data-fields (up to 2^32)
* Each data field has a UTF-8 name, a fixed type, and 1 or more data-values of that type.
* Supported field-data-types include:  `int8`, `int16`, `int32`, `int64`, `float`, `double`, [String](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html), [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html), [Point](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Point.html), [Rect](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Rect.html), plus any arbitrary additional types the user cares to define.
* Each [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) also includes a single `uint32` "what-code", which is typically used for tagging and quick dispatch (e.g. via a `switch` statement)
* A [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) is [Flattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html):  i.e. serializable to a platform-neutral flattened-bytes representation that can be saved to disk or transmitted across the network, and later parsed unambiguously under any OS, CPU, or language.
* A [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) is [RefCountable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RefCountable.html), which allows efficient (zero-copy) in-process transmission of [Messages](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) to other threads and functions, and efficient adding of [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) objects to data structures (like the server-side [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) tree)
* Call [AddInt32()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#a23131d5cc82ba54d48aaa2deda53fb16)/[AddString()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#a25a2686c54cfe4e68c89c04cb6dccf4e)/[AddFloat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#aec9027135529af3b3a9f1acceb4caa02)/[AddMessage()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#a762474c971da9887c82265b1a1d0b7dc)/etc to add field-data to a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html).
* Call [FindInt32()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#a22e6ab0dd377314202ce327c03d089a8)/[FindString()]()/[FindFloat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#a2211494a780405f060077c43e127eeaf)/[FindMessage()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html#adefee9f05d3d8fa5c2db8f207435b0e5)/etc to retrieve field-data from a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html).
* Call `GetInt32()`/`GetString()`/`GetFloat()`/`GetMessage()`/etc to either retrieve field-data from a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html), or retrieve a default value if the requested data doesn't exist.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/message` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [message/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_1_basic_usage.cpp)
* [message/example_2_nested_messages.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_2_nested_messages.cpp)
* [message/example_3_add_archive.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_3_add_archive.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
* [message/example_5_field_iteration.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_5_field_iteration.cpp)
