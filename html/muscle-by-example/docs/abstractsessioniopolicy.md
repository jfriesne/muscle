# muscle::AbstractSessionIOPolicy class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractSessionIOPolicy.html)

```#include "reflector/AbstractSessionIOPolicy.h"```

[AbstractSessionIOPolicy](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractSessionIOPolicy.html) is an abstract base class for an object that can be used to limit the amount of data the sessions of a [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html) can send/receive on any given iteration of the [ServerProcessLoop()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html#a76a1b18af0dacbb6b0a03d5c6433e15e) event-loop.

* Use of I/O policies is 100% optional -- by default (i.e. with no policies installed) the ReflectServer will just perform all the necessary I/O as quickly as possible, which is usually what you want.
* The most commonly used I/O policy is the [RateLimitSessionIOPolicy](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RateLimitSessionIOPolicy.html), which allows you to control the network bandwidth used by the server.
* You can install a different [AbstractSessionIOPolicy](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractSessionIOPolicy.html) on each session object if you want, or have multiple (or even all) session objects share the same [AbstractSessionIOPolicy](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractSessionIOPolicy.html) object.
* Specify an I/O policy for a session's input data-stream by calling [AbstractReflectSession::SetInputPolicy()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractReflectSession.html#a9e2429dbf9271e9d4af946857a442132)
* Specify an I/O policy for a session's output data-stream by calling [AbstractReflectSession::SetOutputPolicy()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractReflectSession.html#ad571c50884405d59dd5a3701b4914f48)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
