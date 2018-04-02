# muscle::PulseNode class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PulseNode.html)

```#include "util/PulseNode.h"```

* `PulseNode` is subclassed by classes that may want to get their `Pulse()` callback-method called at a time they specify.
* An object that subclasses `PulseNode` may reimplement its `GetPulseTime()` method to return a time (in 64-bit `GetRunTime64()`-style microseconds) at which its `Pulse()` method should be called next (or it can return `MUSCLE_TIME_NEVER` if it doesn't currently want `Pulse()` to be called).
* At the scheduled time that was returned by `GetPulseTime()` (or very shortly thereafter) `Pulse()` will be called.
* This only works if the object is attached to event loop whose `PulseNodeManager` (typically the `ReflectServer`) is implemented to perform the necessary `Pulse()` calls.
* If the object changes its mind about when `Pulse()` should be called next, it should call `InvalidatePulseTime()` to let the `PulseNodeManager` know it needs to call `GetPulseNode()` again to get the new next-pulse-scheduled-for time.  (Exception:  this isn't necessary inside `Pulse()` itself, since `GetPulseTime()` will always be called after a call to `Pulse()`)
* Built-in classes that offer `GetPulseTime()/Pulse()` callbacks include:  `ReflectServer`, `ReflectSessionFactory`, `AbstractReflectSession`, `AbstractMessageIOGateway`, `AbstractSessionIOPolicy`
* When reimplementing `GetPulseTime()` and `Pulse()`, be sure to call up to the parent class's implementation of those methods in order not to break the parent-class's internal Pulse-callback-functionality.  (Combine your `GetPulseTime()` result-value with the parent's result-value via `muscleMin()`)
* If a subclass of `PulseNode` wants to offer `PulseNodeManager`-style `GetPulseTime()/Pulse()` service to other objects that it owns, it can do so easily, just by subclassing those objects from `PulseNode` as well, and calling `PutPulseChild()` on each object once, to add the object to its `PulseNodeManager`'s callback-list.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [reflector/example_6_smart_server_with_pulsenode.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_6_smart_server_with_pulsenode.cpp)
* [reflector/example_7_smart_server_with_udp_pingpong.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_7_smart_server_with_udp_pingpong.cpp)
