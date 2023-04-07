# Time Utility Functions [(API)](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html)

```#include "util/TimeUtilityFunctions.h"```

[TimeUtilityFunctions](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html) is a collection of functions for dealing with time (expressed as `uint64s`, representing microseconds)

* [GetRunTime64()](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html#gaf5fb1efed5d3d68fd7524fe8b36a2e1e) returns the current time according to a monotonic clock (unrelated to calendar time) -- useful for program activity scheduling
* [GetCurrentTime64()](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html#ga896d0d400ce934fb4d5a48ff08f4aea6) returns the current time according to the calendar clock -- useful for human-centric time/date calculations
* [Snooze64()](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html#ga87bad26e8e7ecdfbbe70857b64af74e8) sleeps for the specified number of microseconds
* [GetHumanReadableTimeString()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#gad0132099d3a5413f1a0d79ca71b6181e) returns a human-readable date/time string representing a given `uint64` calendar-clock-value
* [ParseHumanReadableTimeString()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga7640ab5ae203b760ba24367ecc132e11) returns the `uint64` value associated with a [GetHumanReadableTimeString()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#gad0132099d3a5413f1a0d79ca71b6181e)-style human-readable date/time string.
* [GetHumanReadableTimeValues()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga5f62fdd7df343b98068c12cf801f1a60) populates a [HumanReadableTimeValues](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HumanReadableTimeValues.html) object with year/month/date/hours/minutes/seconds values representing a given date/time.
* [GetHumanReadableTimeIntervalString()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga6c91bb15c9c1e92661ba4b28c3cd9416) returns a human-readable string representing a specified duration of time.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/timeutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [timeutilityfunctions/example_1_monotonic_clock.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_1_monotonic_clock.cpp)
* [timeutilityfunctions/example_2_calendar_time.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_2_calendar_time.cpp)
* [timeutilityfunctions/example_3_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_3_idioms.cpp)
