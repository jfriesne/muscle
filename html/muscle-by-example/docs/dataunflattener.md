# muscle::DataUnflattener class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html)

```#include "util/DataUnflattener.h"```

A [DataUnflattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html) is a lightweight binary-data-reader object that makes it easy and safe to read in serialized POD data and [Flattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html)/[PseudoFlattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PseudoFlattenable.html) object-bytes from a byte-buffer.

* Instead of copying data-bytes in "by hand" using pointer-arithmetic and [memcpy()](https://linux.die.net/man/3/memcpy)/[ntohl()](https://linux.die.net/man/3/ntohl)/[muscleCopyIn()](https://public.msli.com/lcs/muscle/html/MuscleSupport_8h.html#a8c0d4c8214ec680ee336bf9f72177d77)/etc, you can declare a [DataUnflattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html) on the stack and call [ReadInt32()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html#aab5d0b8b303c6628d958f4347ae74009)/[ReadFloat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html#afb35c3dc483d1bc37832c6605a66835a)/[ReadString()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html#a8d6b457951b23e6239ac866aa76ba279)/[ReadFlat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html#a2705e947fc8bfe4c77598292cbeb2340)/etc on it.
* [DataUnflattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html)'s methods deal with endian-conversion and unaligned-data-read-safety issues.
* [DataUnflattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataUnflattenerHelper.html) is a synonym for [LittleEndianDataUnflattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a73503de7c954417b33838c7886714d3a) (since MUSCLE uses little-endian data by default).
* Reads from a raw/fixed-size array of uint8 or from a [ByteBuffer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html) object.
* [BigEndianDataUnflattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a8b31dc0d02ed3a3239281c4496664435) and [NativeEndianDataUnflattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a013fed93f6066fcdf14dffa1123d8078) classes are also available.
* [Unchecked*DataUnflattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1UncheckedDataUnflattenerHelper.html) classes are available as a more-efficient implementation that can be used by code that has already done its own bounds-checking.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/bytebuffer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [bytebuffer/example_3_endian.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_3_endian.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
* [flattenable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/flattenable/example_1_basic_usage.cpp)
