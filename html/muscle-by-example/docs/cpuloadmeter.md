# muscle::CPULoadMeter class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1CPULoadMeter.html)

```#include "util/CPULoadMeter.h"```

An OS-neutral mechanism for monitoring the CPU-usage of the host computer over time.

* Tells you the percentage of your computer's CPU power that is in use
* If [GetCPULoad()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1CPULoadMeter.html#ab164f6f0a4e577262dd6b710a7616b8a) returns 0.0f, your CPU is completely idle
* If [GetCPULoad()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1CPULoadMeter.html#ab164f6f0a4e577262dd6b710a7616b8a) returns 1.0f, all cores on your CPU are pegged
* If [GetCPULoad()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1CPULoadMeter.html#ab164f6f0a4e577262dd6b710a7616b8a) returns something in between, your CPU is partially utilized.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/cpuloadmeter` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [cpuloadmeter/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/cpuloadmeter/example_1_basic_usage.cpp)
