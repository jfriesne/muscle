# muscle::StdinDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html)

```#include "dataio/StdinDataIO.h"```

* `StdinDataIO` is used to read data from the process's built-in `stdin` stream.
* Under Mac/Linux, `StdinDataIO` is implemented as a simple pass-through a `FileDescriptorDataIO` member-object for `STDIN_FILENO`.
* Under Windows, stdin can't be `select()`'d on, so `StdinDataIO` is implemented using lots of special magic and an internal thread.  The result is that `StdinDataIO` under Windows behaves that same as `StdinDataIO` under other OS's, despite Windows' limitations.
* By default, and data passed to `StdinDataIO::Write()` is thrown away, since writing to `stdin` doesn't really make sense.  However, if you pass `true` to the StdinDataIO constructor's second argument, then data passed to `StdinDataIO::Write()` will be printed to `stdout`.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

* [dataio/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_1_basic_usage.cpp)
* [dataio/example_2_tcp_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_2_tcp_server.cpp)
* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
