# muscle::ByteBuffer class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html)

```#include "util/ByteBuffer.h"```

* Like the `String` class, except instead of a NUL-terminated string, it holds a variable-sized buffer of arbitrary binary bytes.
* Similar to: `QByteBuffer`, `std::vector<uint8_t>`
* Holds up to (2^32) bytes
* Optional helper methods for writing/reading/appending int/float/string/etc values of varying lengths, with specified endian-ness 
* `ByteBuffer` inherits `FlatCountable` for efficient passing around of large binary blobs
* O(1) SwapContents()
* Idiom:  `ByteBufferRef`s are often allocated using `GetByteBufferFromPool(uint32 bufSize)`

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/bytebuffer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [bytebuffer/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_1_basic_usage.cpp)
* [bytebuffer/example_2_bb_pool.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_2_bb_pool.cpp)
* [bytebuffer/example_3_endian.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_3_endian.cpp)
* [dataio/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_4_idioms.cpp)
* [zlibcodec/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zlibcodec/example_1_basic_usage.cpp)
* [zlibutilityfunctions/example_1_byte_buffers.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zlibutilityfunctions/example_1_byte_buffers.cpp)
