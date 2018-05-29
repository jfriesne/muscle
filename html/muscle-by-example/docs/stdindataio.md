# muscle::StdinDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html)

```#include "dataio/StdinDataIO.h"```

[StdinDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html) reads data from the process's built-in `stdin` stream.

* Under Mac/Linux, [StdinDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html) is implemented as a simple pass-through to a [FileDescriptorDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FileDescriptorDataIO.html) member-object for `STDIN_FILENO`.
* Under Windows, stdin can't be `select()`'d on, so [StdinDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html) is implemented using lots of special magic, an internal thread, and a socket-pair.  The result is that [StdinDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html) under Windows behaves that same as [StdinDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html) under other OS's, despite Windows' limitations.
* By default, any data passed to [StdinDataIO::Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html#a1678b51bff36c393e3b279a73b972366) is simply discarded, since writing to `stdin` doesn't really make sense.  However, if you pass `true` to the [StdinDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html) constructor's second argument, then any data passed to [StdinDataIO::Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html#a1678b51bff36c393e3b279a73b972366) will be printed to `stdout` instead.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [dataio/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_1_basic_usage.cpp)
* [dataio/example_2_tcp_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_2_tcp_server.cpp)
* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
