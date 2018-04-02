# muscle::ZLibUtilityFunctions class [(API)](https://public.msli.com/lcs/muscle/html/group__zipfileutilityfunctions.html)

```#include "zlib/ZLibUtilityFunctions.h"```

* The functions in `ZLibUtilityFunctions.h` are convenience functions for quickly deflating or inflating data.
* `DeflateByteBuffer()` takes a `ByteBufferRef` of raw data and returns a corresponding `ByteBufferRef` of deflated data
* `InflateByteBuffer()` takes a `ByteBufferRef` of deflated data (previously produced by `DeflateByteBuffer`) and returns a corresponding `ByteBufferRef` of re-inflated data
* `DeflateMessage()` takes a `MessageRef` and returns a corresponding `MessageRef` containing only a single field of flattened data (but with the same `what` code) (or if the `MessageRef` that was passed in was already deflated, it just returns the passed-in `MessageRef` verbatim)
* `InflateMessage()` takes a `MessageRef` that was previously returned by `DeflateMessage()` and returns a corresponding re-inflated `MessageRef` containing all of the original fields again.  (or if the `MessageRef` that was passed in wasn't a deflated Message, it just returns the passed-in `MessageRef` verbatim)
* `ReadAndDeflateAndWrite` reads raw data from one `DataIO`, deflates it, and writes the deflated data out to another `DataIO`, without ever loading all of the data into RAM at once.  (Useful for deflating large files)
* `ReadAndInflateAndWrite` reads deflated data from one `DataIO`, inflates it, and writes the inflated data out to another `DataIO`, without ever loading all of the data into RAM at once.  (Useful for inflating large files)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/zlibutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [zlibutilityfunctions/example_1_byte_buffers.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zlibutilityfunctions/example_1_byte_buffers.cpp)
* [zlibutilityfunctions/example_2_messages.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zlibutilityfunctions/example_2_messages.cpp)
