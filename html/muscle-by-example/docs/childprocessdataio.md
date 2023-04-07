# muscle::ChildProcessDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html)

```#include "dataio/ChildProcessDataIO.h"```

[ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) is used to launch a child process, and then (optionally) communicate with it via its `stdin` and `stdout` streams.

* At some point after creating the [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) object you'll want to call [LaunchChildProcess()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#adeb5e4f72aa50544d73c0650f75e60e3) on it to actually launch the child process
* After the child process is launched, [Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a0672add49f6c0f410c361f66a9b00ced) sends data to its `stdin` stream, and [Read()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a044c5d6a5890e2e847b8ddc12f02ea48) reads data from its `stdout` stream.
* By default, the [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) will try to get rid of the child process when the [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) object is being destroyed.  You can modify this behavior by calling [SetChildProcessShutdownBehavior()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#ab2425db88d4b75b06edebf09525d9efc).
* [SignalChildProcess()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a0a84d130498da96687a1ec11dac8cc26) will send a UNIX-style signal to the child process (not available under Windows)
* [KillChildProcess()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a12ca6a1352a287caaacb08151b428502) will nuke the child process from orbit.
* [WaitForChildProcessToExit()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a3f997a175988a602a3c95a1726fd0061) will block until the child process has exited.
* Some static convenience-methods like [System()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#af0bae5cc9c6e0851b044e7c753787afb) and [LaunchIndependentChildProcess()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a9eab03cbc1f386c3026c124b7540616d) are declared to let you easily launch "fire and forget" child processes without having to create a [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) object and keep it around.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
