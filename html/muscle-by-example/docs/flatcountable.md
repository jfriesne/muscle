# muscle::FlatCountable class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FlatCountable.html)

```#include "support/FlatCountable.h"```

* Abstract interface representing an object that is both `Flattenable` and `RefCountable`.
* Inherits both of the above-mentioned superclasses.
* `FlatCountable` objects are doubly useful, since they may be added to `Message` objects by-reference (thus avoiding any unnecesary data copying during inter-thread communication) but can also be automatically flattened if necessary, i.e. if the `Message` gets flattened to be sent outside the process's boundaries (e.g. to a file or to the network)
