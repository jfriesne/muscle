# muscle::DataUnflattener class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html)

```#include "util/DataUnflattener.h"```

A [DataUnflattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html) is a lightweight binary-data-reader object that makes it easy and safe to read in POD and Flattenable/PseudoFlattenable data from a serialized byte-buffer.

* Instead of copying data-bytes in "by hand" using pointer-arithmetic and memcpy()/ntohl()/muscleCopyIn()/etc, you can declare a DataUnflattener on the stack and call ReadInt32()/ReadFloat()/ReadString()/ReadFlat()/etc on it.
* DataUnflattener's methods deal with endian-conversion and unaligned-data-read-safety issues.
* DataUnflattener is a synonym for LittleEndianDataUnflattener (since MUSCLE uses little-endian data by default).
* Reads from a raw/fixed-size array of uint8 or from a ByteBuffer object.
* BigEndianDataUnflattener and NativeEndianDataUnflattener classes are also available.
* Unchecked*DataUnfllattener classes are available as a more-efficient implementation that can be used by code that has already done its own bounds-checking.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/bytebuffer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [bytebuffer/example_3_endian.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_3_endian.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
* [flattenable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/flattenable/example_1_basic_usage.cpp)
