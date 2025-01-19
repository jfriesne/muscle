# muscle::OutputPrinter class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1OutputPrinter.html)

```#include "util/OutputPrinter.h"```

[OutputPrinter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1OutputPrinter.html) is a utility class for retargetting printf()-style text output to different destinations.

* Code that otherwise would have called [printf() or fprintf()](https://man7.org/linux/man-pages/man3/fprintf.3.html) can call OutputPrinter::printf() instead.
* By doing so, the code gains the ability to output to stdout, or stderr, or to any stdio FILE handle, or to a [String](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/site/string/), or to the [MUSCLE Logging API](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/site/logtime/), without further modifications.
* OutputPrinter also supports automatic indentation of the generated text lines, for convenient visual nesting of hierarchical output.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/outputprinter` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [outputprinter/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/outputprinter/example_1_basic_usage.cpp)
* [outputprinter/example_2_indentation.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/outputprinter/example_2_indentation.cpp)
