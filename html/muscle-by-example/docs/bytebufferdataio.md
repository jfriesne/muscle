# muscle::ByteBufferDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html)

```#include "dataio/ByteBufferDataIO.h"```

[ByteBufferDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBufferDataIO.html) can be used to access the memory-contents of a [ByteBuffer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html) object as if it were a file.

* Useful if you have some code that was designed to use a [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html), but you'd rather not write to disk/network, and you prefer keep the output in RAM (or read the input from RAM) instead.
* The [ByteBuffer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBuffer.html) will behave much like a file would (i.e. it will be resized larger as you [Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ByteBufferDataIO.html#a1678b51bff36c393e3b279a73b972366) past the end of its current size to append data, etc)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
