# muscle::QueryFilter class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html)

```#include "regex/QueryFilter.h"```

A boolean-predicate-test object for limiting the results of MUSCLE queries to only nodes whose [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html)-payloads meet specified criteria.

* The main purpose for a [QueryFilter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html) object is to constrain a MUSCLE database-node subscription so that only nodes whose [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) objects match specified criteria will be returned.  That way if a client is only interested in certain nodes, the filtering can be done on the server-side to save bandwidth.
* A [QueryFilter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html) represents a pass/fail test that can be applied to a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object
* Given a particular [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object, the [Matches()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html#a599ebe1386b308fc09d8841e18e72f13) method of a [QueryFilter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html) can return true or false.
* [QueryFilter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html) objects have [SaveToArchive()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html#a7047adedb541b4bb26a2c06b227f2a46) and [SetFromArchive()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html#aa18d303693d5038cf902dc37bde95198) methods defined, so that they can be transported across the network.
* [QueryFilters](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html) can be composed into groups (using e.g. [AndQueryFilter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AndQueryFilter.html) or [OrQueryFilter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1OrQueryFilter.html)) to represent arbitrarily complex boolean expressions.
* The [CreateQueryFilterFromExpression()](https://public.msli.com/lcs/muscle/html/namespacemuscle.html#a604e9a5eea9d86ef3e11d440851394d9) function can be used to quickly create an appropriate [QueryFilter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html) object from a human-readable text string.  For example, `CreateQueryFilterFromExpression("(age >= 18) and (weight <= 150.0f)");` would return a [QueryFilter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QueryFilter.html) that checks the `age` and `weight` fields of each [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) to see if they pass the specified tests.  See the "Building a QueryFilter from an expression-String" section of the [Beginner's Guide](https://public.msli.com/lcs/muscle/muscle/html/Beginners%20Guide.html) for details.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/queryfilter` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [queryfilter/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queryfilter/example_1_basic_usage.cpp)
* [queryfilter/example_2_smart_client_with_queryfilter.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queryfilter/example_2_smart_client_with_queryfilter.cpp)
