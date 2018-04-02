# muscle::SignalHandlerSession class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SignalHandlerSession.html)

```#include "reflector/SignalHandlerSession.h"```

* This session can be added to your `ReflectServer` if you want to react to Unix-style signals (SIGINT/SIGHUP/etc) or Windows console events (CTRL_CLOSE_EVENT/CTRL_LOGOFF_EVENT/etc)
* The easiest way to enable it is just to call muscle::SetMainReflectServerCatchSignals(true) before starting your main thread's event loop.  Then `ReflectServer::ServerEventLoop()` will add a `SignalHandlerSession` internally that causes `ServerProcessLoop()` to return when SIGINT/etc is received.
* You can also manually add a `SignalHandlerSession` to the ReflectServer if you want to implement your own custom handling.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/signalhandlersession` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [signalhandlersession/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/signalhandlersession/example_1_basic_usage.cpp)
* [signalhandlersession/example_2_custom_handling.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/signalhandlersession/example_2_custom_handling.cpp)
