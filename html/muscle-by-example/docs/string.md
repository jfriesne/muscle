# muscle::String class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html)

```#include "util/String.h"```

* Holds a NUL-terminated C string (ASCII or UTF-8)
* Similar to: `QString`, `std::string`, `java.util.String`
* Length up to (2^32)-1 bytes
* Short-string optimization:  Strings less than 8 bytes long avoid any heap allocation
* Idiom:  () operator as a shorthand for calling CStr() method to get a `(const char *)`:

```
    const String s = "foobar"; 
    const char * cptr = s();   // s() is shorthand for s.CStr()
    printf("s contains [%s]\n", cptr);
```

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/string` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [string/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/string/example_1_basic_usage.cpp)
* [string/example_2_substrings.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/string/example_2_substrings.cpp)
* [string/example_3_interactive.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/string/example_3_interactive.cpp)
* [string/example_4_interpolation.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/string/example_4_interpolation.cpp)
