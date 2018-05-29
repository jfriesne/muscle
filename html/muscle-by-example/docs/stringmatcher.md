# muscle::StringMatcher class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StringMatcher.html)

```#include "regex/StringMatcher.h"```

Holds a pattern-string (ASCII or UTF-8) that represents a bash-shell-style wildcard/globbing-pattern, and uses it to do pattern-matching.

* [StringMatcher::Match(const char *)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StringMatcher.html#ad0c0827dee710b8f323defdb4de7d315) returns true iff the pattern-string matches the passed-in string.
* Similar to: [QRegularExpression](http://doc.qt.io/qt-5/qregularexpression.html), [std::regex_match](http://en.cppreference.com/w/cpp/regex/regex_match)
* For example, glob-pattern `str*` will match "string" and "strap" but not "ring" or "trap".
* Supported "traditional" wildcard-characters include `*`, `?`, `[`, `]`, `\`, `,`, `(`, `)`
* Syntax extension:  a glob-pattern starting with `~` will match only strings that *don't* match the rest of the pattern (e.g. `~j*` will match all strings that *don't* start with "j")
* Syntax extension:  a glob-pattern like `<3-5,10-12,20->` will match strings that represent integers in the specified range(s) (e.g. "3", "4", "5", "10", "11", and "12", plus "20" and higher)
* [StringMatcher](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StringMatcher.html) can also be used to match against standard regex-patterns, if the simplified bash-shell-style globbing-syntax isn't sufficient.  (pass in `false` as the second constructor-argument to enable full-regex mode)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/stringmatcher` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [stringmatcher/example_1_glob_matching.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/stringmatcher/example_1_glob_matching.cpp)
* [stringmatcher/example_2_regex_matching.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/stringmatcher/example_2_regex_matching.cpp)
