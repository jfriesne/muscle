# muscle::SignalHandlerSession class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SignalHandlerSession.html)

```#include "reflector/SignalHandlerSession.h"```

[SignalHandlerSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SignalHandlerSession.html) is an OS-neutral mechanism for catching and reacting to asynchronous signal events.

* This session can be added to your [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html) if you want to react to Unix-style signals (SIGINT/SIGHUP/etc) or Windows console events (CTRL_CLOSE_EVENT/CTRL_LOGOFF_EVENT/etc)
* The easiest way to enable it is just to call [muscle::SetMainReflectServerCatchSignals(true)](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#aebb53c752cb4fa1eb4c0fb019d8ec89f) before starting your main thread's event loop.  Then [ReflectServer::ServerProcessLoop()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html#a76a1b18af0dacbb6b0a03d5c6433e15e) will add a [SignalHandlerSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SignalHandlerSession.html) internally that causes [ReflectServer::ServerProcessLoop()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html#a76a1b18af0dacbb6b0a03d5c6433e15e) to return when SIGINT/etc is received.
* You can also manually add a [SignalHandlerSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SignalHandlerSession.html) to the ReflectServer if you want to implement your own custom handling.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/signalhandlersession` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [signalhandlersession/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/signalhandlersession/example_1_basic_usage.cpp)
* [signalhandlersession/example_2_custom_handling.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/signalhandlersession/example_2_custom_handling.cpp)
