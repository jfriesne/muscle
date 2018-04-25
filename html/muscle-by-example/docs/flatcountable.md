# muscle::FlatCountable class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FlatCountable.html)

```#include "support/FlatCountable.h"```

Abstract interface representing an object that is both [Flattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Flattenable.html) and [RefCountable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RefCountable.html).

* Inherits both of the above-mentioned superclasses.
* [FlatCountable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1FlatCountable.html) objects are doubly useful, since they may be added to [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) objects by-reference (thus avoiding any unnecesary data copying during inter-thread communication) but can also be automatically flattened if necessary, i.e. if the [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) gets flattened to be sent outside the process's boundaries (e.g. to a file or to the network)
