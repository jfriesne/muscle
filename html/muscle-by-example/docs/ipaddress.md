# muscle::IPAddress class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddress.html)

```#include "util/IPAddress.h"```

* Holds an IPv4 or IPv6 numeric address.
* Similar to: `QHostAddress`
* IPAddress includes scope-index (used for link-local IPv6 addresses only)
* Inherits [PseudoFlattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PseudoFlattenable.html) for easy archiving/transmission
* [IPAddressAndPort](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddressAndPort.html) contains an IPAddress object plus a port-number
* `IPAddress` and `IPAddressAndPort` include `ToString()` and `FromString()` methods for easy conversion to (and from) user-readable `Strings`.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/ipaddress` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [ipaddress/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/ipaddress/example_1_basic_usage.cpp)
* [ipaddress/example_2_interactive_ipaddress.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/ipaddress/example_2_interactive_ipaddress.cpp)
* [ipaddress/example_3_interactive_ipaddress_and_port.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/ipaddress/example_3_interactive_ipaddress_and_port.cpp)
