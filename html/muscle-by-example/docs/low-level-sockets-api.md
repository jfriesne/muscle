## Low-Level Sockets [(API)](https://public.msli.com/lcs/muscle/html/group__networkutilityfunctions.html)

```#include "util/NetworkUtilityFunctions.h"```

MUSCLE's Low-Level Sockets API is very similar to the
Berkeley BSD Sockets API, except it uses [ConstSocketRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstSocketRef.html)
objects instead of `int` objects for file descriptors,
so there is almost no possibility of 'leaking' file
descriptors.

Also it is much easier to use correctly than the BSD sockets API
(at the cost of being applicable to IPv4 and IPv6 only
-- if you need to use some other obscure type of networking
layer like DECnet, then most of the functions in `NetworkUtilityFunctions.h` won't help you much)
