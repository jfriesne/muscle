# muscle::StringTokenizer class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StringTokenizer.html)

```#include "util/StringTokenizer.h"```

MUSCLE's class for iterating over substrings in a string.

* Given a C-string, parses out the contained words or clauses in order
* Similar to: [strtok()](http://man7.org/linux/man-pages/man3/strtok.3.html), [java.util.StringTokenizer](https://docs.oracle.com/javase/8/docs/api/index.html?java/util/StringTokenizer.html), [boost::tokenizer](https://www.boost.org/doc/libs/1_66_0/libs/tokenizer/tokenizer.htm)
* More than one token-separator-char can be specified (e.g. spaces AND commas)
* By default, separator-chars are "soft":  multiple adjacent soft-separator-chars will be treated as if they were a single separator-char.  (Useful for whitespace)
* If you specify a separator-char more than once in the constructor-argument, it will become a "hard" separator; multiple adjacent hard-separator-chars will be treated as separating empty strings.

e.g. this code:

```
    const char * parseMe = "one,two, three ,,four five";

    StringTokenizer tok(parseMe, ",, ");  // comma is deliberately specified twice to make it a "hard separator char"

    const char * nextTok;
    while((nextTok = tok()) != NULL)
    {
       printf("[%s]\n", nextTok);
    }
```

will print:

```
[one]
[two]
[three]
[]
[four]
[five]
```

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/stringtokenizer` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [stringtokenizer/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/stringtokenizer/example_1_basic_usage.cpp)
