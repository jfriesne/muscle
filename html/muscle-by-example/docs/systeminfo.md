# muscle::SystemInfo functions [(API)](https://public.msli.com/lcs/muscle/html/group__systeminfo.html)

```#include "system/SystemInfo.h"```

Functions that return gestalt information about the environment the program is running in.

* [GetOSName()](https://public.msli.com/lcs/muscle/html/group__systeminfo.html#gad7d0acc9a16fb86a5c14c00b91fc8f9a) returns a string representing the OS the executable was compiled on.
* [GetSystemPath()](https://public.msli.com/lcs/muscle/html/group__systeminfo.html#ga87ea0d38a67f49e55c47332afd6e0513) returns the filesystem-paths to various resources (executable path, desktop path, etc)
* [GetNumberOfProcessors()](https://public.msli.com/lcs/muscle/html/group__systeminfo.html#ga6ad7df1341276b1a9545cc3022250a35) returns the number of CPU cores on the computer we are running on
* etc

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/systeminfo` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [systeminfo/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/systeminfo/example_1_basic_usage.cpp)
