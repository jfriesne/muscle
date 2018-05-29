# muscle::ByteBuffer class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html)

```#include "util/ByteBuffer.h"```

A [ByteBuffer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html) holds a variable-sized array of raw unsigned bytes.

* Similar to the [String](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html) class, except instead of a 0-terminated string, it holds raw binary data.
* Similar to: [QByteBuffer](http://doc.qt.io/qt-5/qbytearray.html), [std::vector&lt;uint8_t&gt;](http://en.cppreference.com/w/cpp/container/vector)
* Holds up to (2^32) bytes
* Has convenience methods for writing/reading/appending `int32`/`float`/[String](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html)/etc values of varying lengths, with specified endian-ness 
* [ByteBuffer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html) inherits [FlatCountable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FlatCountable.html) for efficient passing around of large binary blobs
* O(1) [SwapContents()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html#a837d6130df99b279da586edd767230c2)
* Idiom:  [ByteBufferRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html)s are often allocated using [GetByteBufferFromPool()](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a527bec3824435111b6abb9a75cb1e305)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/bytebuffer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [bytebuffer/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_1_basic_usage.cpp)
* [bytebuffer/example_2_bb_pool.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_2_bb_pool.cpp)
* [bytebuffer/example_3_endian.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_3_endian.cpp)
* [dataio/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_4_idioms.cpp)
* [zlibcodec/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zlibcodec/example_1_basic_usage.cpp)
* [zlibutilityfunctions/example_1_byte_buffers.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zlibutilityfunctions/example_1_byte_buffers.cpp)
