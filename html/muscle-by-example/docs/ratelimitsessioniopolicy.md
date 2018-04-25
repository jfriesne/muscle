# muscle::RateLimitSessionIOPolicy class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RateLimitSessionIOPolicy.html)

```#include "reflector/RateLimitSessionIOPolicy.h"```

[RateLimitSessionIOPolicy](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RateLimitSessionIOPolicy.html) is an [AbstractSessionIOPolicy](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractSessionIOPolicy.html) that you can install on a session to cap its incoming- and/or outgoing-bandwidth usage to a set maximum.

* You can install a separate [RateLimitSessionIOPolicy](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RateLimitSessionIOPolicy.html) on each session in order to implement a per-session bandwidth cap
* Alternatively, you could instantiate a single [RateLimitSessionIOPolicy](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RateLimitSessionIOPolicy.html) object and install it on all sessions to enforce a cumulative bandwidth cap across the whole [ReflectServer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ReflectServer.html).
* Or you could aim somewhere in the middle, with certain groups of sessions sharing a bandwidth cap while other sessions operate freely (or etc).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
