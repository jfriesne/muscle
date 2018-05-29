# muscle::ChildProcessDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html)

```#include "dataio/ChildProcessDataIO.h"```

[ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) is used to launch a child process, and then (optionally) communicate with it via its `stdin` and `stdout` streams.

* At some point after creating the [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) object you'll want to call [LaunchChildProcess()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a58ece1d0c75d2eecb09d2eede36ae7c1) on it to actually launch the child process
* After the child process is launched, [Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a1678b51bff36c393e3b279a73b972366) sends data to its `stdin` stream, and [Read()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#ab13ddbab8904694a9b7f621ee0b4fc78) reads data from its `stdout` stream.
* By default, the [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) will try to get rid of the child process when the [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) object is being destroyed.  You can modify this behavior by calling [SetChildProcessShutdownBehavior()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#ab2425db88d4b75b06edebf09525d9efc).
* [SignalChildProcess()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a0a84d130498da96687a1ec11dac8cc26) will send a UNIX-style signal to the child process (not available under Windows)
* [KillChildProcess()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a12ca6a1352a287caaacb08151b428502) will nuke the child process from orbit.
* [WaitForChildProcessToExit()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#ae447cec695bee3f6bd709cd6b96746a4) will block until the child process has exited.
* Some static convenience-methods like [System()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a887e413ab36280152261d03755c1c23b) and [LaunchIndependentChildProcess()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html#a28d6056323735fa13b790ac3f0a79eec) are declared to let you easily launch "fire and forget" child processes without having to create a [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html) object and keep it around.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
