# muscle::SystemInfo functions [(API)](https://public.msli.com/lcs/muscle/html/group__systeminfo.html)

```#include "system/SystemInfo.h"```

Functions that return gestalt information about the environment the program is running in.

* [GetOSName()](https://public.msli.com/lcs/muscle/html/group__systeminfo.html#ga262b285be03a99350d609c43ef0fbc58) returns a string representing the OS the executable was compiled on.
* [GetSystemPath()](https://public.msli.com/lcs/muscle/html/group__systeminfo.html#ga560d029e2268e9c2716de9b15deb05cc) returns the filesystem-paths to various resources (executable path, desktop path, etc)
* [GetNumberOfProcessors()](https://public.msli.com/lcs/muscle/html/group__systeminfo.html#gab9074b13061a6d579f5ce32f04a2358c) returns the number of CPU cores on the computer we are running on
* etc

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/systeminfo` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [systeminfo/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/systeminfo/example_1_basic_usage.cpp)
