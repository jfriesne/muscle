# muscle::DataFlattener class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html)

```#include "util/DataFlattener.h"```

A [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) is a lightweight binary-data-writer object that makes it easy and safe to write POD and Flattenable/PseudoFlattenable data out to a serialized byte-buffer.

* Instead of copying data-bytes out "by hand" using pointer-arithmetic and memcpy()/htonl()/muscleCopyOut()/etc, you can declare a DataFlattener on the stack and call WriteInt32()/WriteFloat()/WriteString()/WriteFlat()/etc on it.
* DataFlattener's methods deal with endian-conversion and unaligned-data-write-safety issues.
* DataFlattener is a synonym for LittleEndianDataFlattener (since MUSCLE uses little-endian data by default).
* Writes to a raw/fixed-size array of uint8, or can be used together with a ByteBuffer to enable automatic-output-buffer-resizing as necessary.
* BigEndianDataFlattener and NativeEndianDataFlattener classes are also available.
* Unchecked*DataFlattener classes are available as a more-efficient implementation that can be used by code that has already done its own bounds-checking.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/bytebuffer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [bytebuffer/example_3_endian.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_3_endian.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
* [flattenable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/flattenable/example_1_basic_usage.cpp)
