# muscle::QueryFilter class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html)

```#include "regex/QueryFilter.h"```

* The main purpose for a `QueryFilter` object is to constrain a MUSCLE database-node subscription so that only nodes whose `Message` objects match specified criteria will be returned.  That way if a client is only interested in certain nodes, the filtering can be done on the server-side to save bandwidth.
* A `QueryFilter` represents a pass/fail test that can be applied to a `Message` object
* Given a particular `Message` object, the `Matches()` method of a QueryFilter can return true or false.
* `QueryFilter` objects have `SaveToArchive()` and `SetFromArchive()` methods defined, so that they can be transported across the network.
* QueryFilters can be composed into groups (using e.g. `AndOrQueryFilter`) to represent arbitrarily complex boolean expressions.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/queryfilter` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [queryfilter/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queryfilter/example_1_basic_usage.cpp)
* [queryfilter/example_2_smart_client_with_queryfilter.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queryfilter/example_2_smart_client_with_queryfilter.cpp)
