# muscle::StackTrace class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StackTrace.html)

```#include "system/StackTrace.h"```

An OS-neutral API for creating, storing, and printing out a stack-trace of the current thread.  Useful for debugging.

* For the most common use-case (e.g. you just want to print the current stack-trace) just call [PrintStackTrace()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga99dbdc74c85bd12a2689cc6b8b7cad0e) or e.g. [PrintStackTrace(stderr)](https://public.msli.com/lcs/muscle/html/group__systemlog.html#gacff5f1024328bd64a7564a06819c8008).
* If you want to just store a stack trace for later (without immediately printing it) you can instead create a [StackTrace](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StackTrace.html) object and call its [CaptureStackFrames()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StackTrace.html#afeb6d3e2761a967caf8daa6d0d1a0fda) method.
* Later, when you do want to print out the previously-captured stack trace, you can call [Print()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StackTrace.html#a837a6327def4c7d922b38bb6c42b32f7) on it to print out its contents to a stream or to a [String](https://public.msli.com/lcs/muscle/html/classmuscle_1_1String.html).
