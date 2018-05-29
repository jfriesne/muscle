# muscle::IPAddress class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddress.html)

```#include "util/IPAddress.h"```

Holds a numeric IPv4 or IPv6 address.

* Similar to: [QHostAddress](http://doc.qt.io/qt-5/qhostaddress.html)
* Inherits [PseudoFlattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PseudoFlattenable.html) for easy archiving/transmission
* [IPAddress](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddress.html) also includes a scope-index (used for link-local IPv6 addresses only)
* [IPAddressAndPort](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddressAndPort.html) contains an IPAddress object plus a port-number
* [IPAddress](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddress.html) and [IPAddressAndPort](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddressAndPort.html) include [ToString()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddressAndPort.html#afd7d1d2072436a5eafd073eac279a319) and [SetFromString()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1IPAddressAndPort.html#a9e6ecb8dee009f6d934b5ae378828152) methods for easy conversion to (and from) user-readable [Strings](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/ipaddress` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [ipaddress/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/ipaddress/example_1_basic_usage.cpp)
* [ipaddress/example_2_interactive_ipaddress.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/ipaddress/example_2_interactive_ipaddress.cpp)
* [ipaddress/example_3_interactive_ipaddress_and_port.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/ipaddress/example_3_interactive_ipaddress_and_port.cpp)
