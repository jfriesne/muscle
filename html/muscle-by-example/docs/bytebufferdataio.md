# muscle::ByteBufferDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html)

```#include "dataio/ByteBufferDataIO.h"```

* `ByteBufferDataIO` can be used to access the memory-contents of a `ByteBuffer` object as if it were a file.
* Useful if you have some DataIO-based code that was designed to write to disk, but you'd rather not write to disk, and you prefer keep the output in RAM (or read the input from RAM) instead.
* The ByteBuffer will behave much like a file would (i.e. it will be resized larger as you `Write()` past the end of its current size to append data, etc)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
