# muscle::DataFlattener class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html)

```#include "util/DataFlattener.h"```

A [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) is a lightweight binary-data-writer object that makes it easy and safe to write/serialize POD data and [Flattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html)/[PseudoFlattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PseudoFlattenable.html) objects out to a fixed-size byte-buffer.

* Instead of copying data-bytes out "by hand" using pointer-arithmetic and [memcpy()](https://linux.die.net/man/3/memcpy)/[htonl()](https://linux.die.net/man/3/htonl)/[muscleCopyOut()](https://public.msli.com/lcs/muscle/html/MuscleSupport_8h.html#adcf13d828e23ecf7c302b34a8617e235)/etc, you can declare a [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) on the stack and call [WriteInt32()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html#a5f25f22e57bcb7d300cce4b8f453625f)/[WriteFloat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html#a4cb83562a6bbd68fffd626171ed17d88)/[WriteString()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html#a425e99a44fd1206a0301e846609b52fe)/[WriteFlat()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html#a9f90f734d0699ddac3ad0e3fe809986a)/etc on it.
* [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html)'s methods deal with endian-conversion and unaligned-data-write-safety issues.
* [DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataFlattenerHelper.html) is a synonym for [LittleEndianDataFlattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a543a1f7651c0197f4424b72ccdf05702) (since MUSCLE uses little-endian data by default).
* [Checked*DataFlattener](https://public.msli.com/lcs/muscle/html/classmuscle_1_1CheckedDataFlattenerHelper.html) classes that do per-method-call bounds-checking are also available, and they can automatically resize a [ByteBuffer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html) larger as data is written.
* [BigEndianDataFlattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#af031bd6eedaa92a4d49189fe52e4ccf1) and [NativeEndianDataFlattener](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a541e3ed88d0bf734b4e3b8c32f96b225) classes are also available.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/bytebuffer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [bytebuffer/example_3_endian.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/bytebuffer/example_3_endian.cpp)
* [message/example_4_add_flat.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/message/example_4_add_flat.cpp)
* [flattenable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/flattenable/example_1_basic_usage.cpp)
