# muscle::RateLimitSessionIOPolicy class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RateLimitSessionIOPolicy.html)

```#include "reflector/RateLimitSessionIOPolicy.h"```

* `RateLimitSessionIOPolicy` is an `AbstractSessionIOPolicy` that you can install on a session to cap its bandwidth usage to a set maximum.
* You can install a separate `RateLimitSessionIOPolicy` on each session in order to implement a per-session bandwidth cap
* Alternatively, you could instantiate a single `RateLimitSessionIOPolicy` object and install it on all sessions to enforce a cumulative bandwidth cap for the whole `ReflectServer`.
* Or you could do something in the middle, with various groups of session sharing bandwidth caps but operating independently of other groups of sessions (or etc).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
