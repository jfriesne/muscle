# Time Utility Functions [(API)](https://public.msli.com/lcs/muscle/html/group__timeutilityfunctions.html)

```#include "util/TimeUtilityFunctions.h"```

* Various functions for dealing with time (expressed as 64-bit integers, primarily in units of microseconds)
* `GetRunTime64()` returns the current time according to a monotonic clock (unrelated to calendar time) -- useful for program activity scheduling
* `GetCurrentTime64()` returns the current time according to the calendar clock -- useful for human-centric time/date calculations
* `Snooze64()` sleeps for the specified number of microseconds
* `GetHumanReadableTimeString()` returns a human-readable date/time string for a given `uint64` calendar-clock-value
* `ParseHumanReadableTimeString()` returns the `uint64` value associated with a `GetHumanReadableTimeString()`-style string.
* `GetHumanReadableTimeValues()` populates a `HumanReadableTimeValues` object with year/month/date/hours/minutes/seconds values representing a given date/time.
* `GetHumanReadableTimeIntervalString()` returns a human-readable string representing a specified duration of time.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/timeutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [timeutilityfunctions/example_1_monotonic_clock.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_1_monotonic_clock.cpp)
* [timeutilityfunctions/example_2_calendar_time.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_2_calendar_time.cpp)
* [timeutilityfunctions/example_3_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/timeutilityfunctions/example_3_idioms.cpp)
