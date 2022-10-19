# muscle::ByteUnflattener class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteUnflattenerHelper.html)

```#include "util/ByteUnflattener.h"```

A [ByteUnflattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteUnflattenerHelper.html) is a lightweight binary-data-reader object that makes it easy and safe to read in POD data values from a serialized byte-buffer.

* Instead of copying data-bytes in "by hand" using pointer-arithmetic and memcpy()/ntohl()/etc, you can declare a ByteUnflattener on the stack and call ReadInt32()/ReadFloat()/ReadString()/ReadFlat()/etc on it.
* ByteUnflattener's methods deal with endian-conversion and unaligned-data-read-safety issues.
* ByteUnflattener is a synonym for LittleEndianByteUnflattener (since MUSCLE uses little-endian data by default).
* BigEndianByteUnflattener and NativeEndianByteUnflattener classes are also available.
* Reads from a raw/fixed-size array of uint8 or from a ByteBuffer object.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/bytebuffer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [bytebuffer/example_3_endian.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_3_endian.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
* [flattenable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/flattenable/example_1_basic_usage.cpp)
