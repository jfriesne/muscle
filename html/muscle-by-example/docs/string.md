# muscle::String class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html)

```#include "util/String.h"```

Holds a resizable array of `chars` that contains a 0-terminated C-string (ASCII or UTF-8).

* Similar to: [QString](http://doc.qt.io/qt-5/qstring.html), [std::string](http://en.cppreference.com/w/cpp/string/basic_string), [java.util.String](https://docs.oracle.com/javase/8/docs/api/java/lang/String.html)
* Supports string lengths up to (2^32)-1 bytes
* Short-string optimization:  Strings less than 8 bytes long require no heap allocations
* Idiom:  [() operator](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html#aedbbb225272c3e6e225682a23562206e) is a shorthand for calling [Cstr()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html#af1598eaf8f566523418a31cb9c855c83) method to get a `(const char *)`:

```
    const String s = "foobar";
    const char * cptr = s();   // s() is shorthand for s.Cstr()
    printf("s contains [%s]\n", cptr);
```

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/string` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [string/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/string/example_1_basic_usage.cpp)
* [string/example_2_substrings.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/string/example_2_substrings.cpp)
* [string/example_3_interactive.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/string/example_3_interactive.cpp)
* [string/example_4_interpolation.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/string/example_4_interpolation.cpp)
