# muscle::ChildProcessDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html)

```#include "dataio/ChildProcessDataIO.h"```

* `ChildProcessDataIO` is used to launch a child process, and then (optionally) communicate with it via its `stdin` and `stdout` streams.
* At some point after creating the `ChildProcessDataIO` object you'll want to call `LaunchChildProcess(...)` on it to actually launch the child process
* After the child process is launched, `Write()` sends data to its `stdin` stream, and `Read()` reads data from its `stdout` stream.
* By default, the `ChildProcessDataIO` will try to get rid of the child process when the `ChildProcessDataIO` object is being destroyed.  You can modify this behavior by calling `SetChildProcessShutdownBehavior()`.
* `SignalChildProcess()` will send a UNIX-style signal to the child process (not available under Windows)
* `KillChildProcess()` will nuke the child process from orbit.
* `WaitForChildProcessToExit()` will block until the child process has exited.
* Some static convenience-methods like `System()` and `LaunchIndependentChildProcess()` are declared to let you easily launch child processes without having to create and keep a `ChildProcessDataIO` object around.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
