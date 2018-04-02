# muscle::StringMatcher class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StringMatcher.html)

```#include "regex/StringMatcher.h"```

* Holds a string (ASCII or UTF-8) representing a bash-shell-style globbing-pattern.
* `Match(const char *)` method returns true iff the given value-string is matched by the StringMatcher's pattern-string.
* Similar to: `QRegExp`, `std::regex_match`
* For example, glob-pattern `str*` will match "string" and "strap" but not "ring" and "trap".
* Syntax extension:  a glob-pattern like `<3-5,10-12,20->` will match strings that represent integers in the specified range(s) (e.g. "3", "4", "5", "10", "11", and "12", plus "20" and higher)
* Syntax extension:  a glob-pattern starting with a `~` will match only strings that *don't* match the rest of the pattern (e.g. `~j*` will match all strings that *don't* start with j)
* Can also be used to match against regex patterns directly, if the simplified bash-shell-style globbing-syntax isn't sufficient.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/stringmatcher` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [stringmatcher/example_1_glob_matching.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/stringmatcher/example_1_glob_matching.cpp)
* [stringmatcher/example_2_regex_matching.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/stringmatcher/example_2_regex_matching.cpp)
