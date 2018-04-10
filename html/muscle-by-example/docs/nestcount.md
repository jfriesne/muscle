# muscle::NestCount class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NestCount.html)

```#include "util/NestCount.h"```

The `NestCount` class is a simple recursion-safe RAII counter mechanism for tracking a program's execution-state within a call tree.  

* A `NestCount` object is typically declared as a member variable in a class, and a `NestCountGuard` object is declared at the top of a (potentially) recursive/re-entrant method in that class.
* Once that is done, the `NestCount`'s `IsInBatch()` method can be called at any time to find out if any `NestCountGuard`s referencing that `NestCount` are currently on the thread's stack.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/nestcount` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [nestcount/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/nestcount/example_1_basic_usage.cpp)
* [nestcount/example_2_recursion_batch.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/nestcount/example_2_recursion_batch.cpp)
* [nestcount/example_3_without_guard.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/nestcount/example_3_without_guard.cpp)