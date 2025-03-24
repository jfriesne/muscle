# muscle::DataFlattener class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html)

```#include "util/DataFlattener.h"```

A [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) is a lightweight binary-data-writer object that makes it easy and safe to write/serialize POD data and [Flattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html)/[PseudoFlattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PseudoFlattenable.html) objects out to a fixed-size byte-buffer.

* Instead of copying data-bytes out "by hand" using pointer-arithmetic and [memcpy()](https://linux.die.net/man/3/memcpy)/[htonl()](https://linux.die.net/man/3/htonl)/[muscleCopyOut()](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#adcf13d828e23ecf7c302b34a8617e235)/etc, you can declare a [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) on the stack and call [WriteInt32()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html#a82de116d68a9dc994a85ae703aa63c32)/[WriteFloat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html#a25f288a05d29d59bc5e3bb0b5a1b3ee4)/[WriteFlat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html#a51cce295fcb936a8407a74342ce6dd09)/etc on it.
* [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html)'s methods automatically handle endian-conversion and unaligned-data-write-safety issues.
* [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) contains logic to detect when the calling code didn't write the expected number of bytes, and [notify the programmer via an assertion failure](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html#ada1dcaf93508bf7af601806002da2b9c) when that happens.
* [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) is a synonym for [LittleEndianDataFlattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a6a1be468423189772ee19444d6b1cb39) (since MUSCLE uses little-endian data by default).
* [BigEndianDataFlattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a6bfa13c9941a6bdea7e8029506619c6f) and [NativeEndianDataFlattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a2d54be664a90cca70974b2c39ee99746) classes are also available.
* [Checked*DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1CheckedDataFlattenerHelper.html) classes that do per-method-call bounds-checking are also available, and they can automatically resize a [ByteBuffer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html) larger as data is written.
* [DataIOFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IODataFlattenerHelper.html) is similar to [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) except it uses a [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) object to write out data, instead of writing directly to an in-memory byte-array.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/bytebuffer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [bytebuffer/example_3_endian.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_3_endian.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
* [flattenable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/flattenable/example_1_basic_usage.cpp)
