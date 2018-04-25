# muscle::SeekableDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html)

```#include "dataio/SeekableDataIO.h"```

[SeekableDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html) is an interface that extends the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) interface to handle seekable devices.

* A [SeekableDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html) can do the usual [Read()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#a4793574f952157131382b248886b136f), [Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#a918476c9b709cc5587054c978e6ee721), etc, but also has the concept of a seekable-head-position within whatever file (or file-like object) it is reading/writing.
* This sub-interface adds [Seek()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html#a4cdfeed9ee8768c7ff905497bab21fbe), [GetPosition()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html#a83eaa2859b7e03d3f412e9af7c02b8c7), and [GetLength()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html#ab582bf8d30dec8d6d6a0252390469986) methods to the [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) API.
* Subclasses of [SeekableDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html) include [FileDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDataIO.html), [FileDescriptorDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDescriptorDataIO.html), and [ByteBufferDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBufferDataIO.html).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_1_basic_usage.cpp)
* [dataio/example_3_seekable_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_3_seekable_dataio.cpp)
* [dataio/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_4_idioms.cpp)
