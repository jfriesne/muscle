# muscle::SystemInfo functions [(API)](https://public.msli.com/lcs/muscle/html/group__systeminfo.html)

```#include "system/SystemInfo.h"```

* Functions that return gestalt information about the environment the program is running in.
* `GetOSName()` returns a string representing the OS we are running under
* `GetSystemPath()` returns the filesystem-paths to various resources (executable path, desktop path, etc)
* `GetNumberOfProcessors()` returns the number of CPU cores on the computer we are running on
* etc

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/systeminfo` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [systeminfo/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/systeminfo/example_1_basic_usage.cpp)
