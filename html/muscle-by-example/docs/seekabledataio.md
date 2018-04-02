# muscle::SeekableDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html)

```#include "dataio/SeekableDataIO.h"```

* `SeekableDataIO` is an Abstract Interface that extends the `DataIO` interface.
* A `SeekableDataIO` can do the usual `Read()`, `Write()`, etc, but also has the concept of a seekable-head-position within whatever file (or file-like object) it is reading/writing.
* This sub-interface adds `Seek()`, `GetPosition()`, and `GetLength()` methods to the `DataIO` API.
* Subclasses of `SeekableDataIO` include [FileDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDataIO.html), [FileDescriptorDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDescriptorDataIO.html), and [ByteBufferDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBufferDataIO.html).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_1_basic_usage.cpp)
* [dataio/example_3_seekable_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_3_seekable_dataio.cpp)
* [dataio/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_4_idioms.cpp)
