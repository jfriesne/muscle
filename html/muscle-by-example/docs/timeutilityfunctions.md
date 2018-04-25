# Time Utility Functions [(API)](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html)

```#include "util/TimeUtilityFunctions.h"```

[TimeUtilityFunctions](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html) is a collection of functions for dealing with time (expressed as `uint64s`, representing microseconds)

* [GetRunTime64()](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html#ga37279a5f229f0402eee6f83d696ea9aa) returns the current time according to a monotonic clock (unrelated to calendar time) -- useful for program activity scheduling
* [GetCurrentTime64()](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html#ga1c086057df9d43cead4397a2dd311777) returns the current time according to the calendar clock -- useful for human-centric time/date calculations
* [Snooze64()](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html#gaea40c516287c3b38e7647e8bd678c914) sleeps for the specified number of microseconds
* [GetHumanReadableTimeString()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#gacffe232a98522a203739a1efd2196bae) returns a human-readable date/time string representing a given `uint64` calendar-clock-value
* [ParseHumanReadableTimeString()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga98c478ec76226024377aa7c11c0703c2) returns the `uint64` value associated with a [GetHumanReadableTimeString()]()-style human-readable date/time string.
* [GetHumanReadableTimeValues()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#gac9e852e8e96bb6a76b81a9edea3bd77c) populates a [HumanReadableTimeValues](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HumanReadableTimeValues.html) object with year/month/date/hours/minutes/seconds values representing a given date/time.
* [GetHumanReadableTimeIntervalString()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga02981795f829ed3ece63c5428c98df32) returns a human-readable string representing a specified duration of time.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/timeutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [timeutilityfunctions/example_1_monotonic_clock.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_1_monotonic_clock.cpp)
* [timeutilityfunctions/example_2_calendar_time.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_2_calendar_time.cpp)
* [timeutilityfunctions/example_3_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_3_idioms.cpp)
