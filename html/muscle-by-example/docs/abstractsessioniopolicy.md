# muscle::AbstractSessionIOPolicy class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractSessionIOPolicy.html)

```#include "reflector/AbstractSessionIOPolicy.h"```

* `AbstractSessionIOPolicy` is an abstract base class for an object that can be used to limit the amount of data the sessions of a `ReflectServer` can send/receive on any given iteration of the `ServerProcessLoop()` event-loop.
* Use of I/O policies is 100% optional -- by default (i.e. with no policies installed) the ReflectServer will just perform as much I/O as possible, as quickly as possible, which is usually what you want.
* The most commonly used I/O policy is the `RateLimitSessionIOPolicy`, which allows you to control the network bandwidth used by the server.
* You can install a different `AbstractSessionIOPolicy` on each session object if you want, or have multiple (or even all) session objects share the same `AbstractSessionIOPolicy` object.
* Specify an I/O policy for a session's input data-stream by calling `AbstractReflectSession::SetInputPolicy()`
* Specify an I/O policy for a session's output data-stream by calling `AbstractReflectSession::SetOutputPolicy()`

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
