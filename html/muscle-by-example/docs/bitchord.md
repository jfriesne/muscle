# muscle::BitChord class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1BitChord.html)

```#include "support/BitChord.h"```

This templated class holds a fixed-size ordered set of boolean bits (packed into 32-bit words), for easy storage and manipulation of an N-bit boolean vector.

* Designed as a more-strongly-typed, less-error-prone replacement for C-style direct bit-manipulation operations
* No limitation on how many bits can be stored
* Implements the [PseudoFlattenable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PseudoFlattenable.html) interface for easy serialization.
* Particularly useful in conjunction with an enumeration of flag-names (e.g. `enum {BIT_A=0, BIT_B, BIT_C, NUM_BITS}; DECLARE_BITCHORD_FLAGS_TYPE(MyBits, NUM_BITS);}`)
* e.g. instead of `uint32 bits = (1<<BIT_A)|(1<<BIT_C);` you could declare `MyBits bits(BIT_A, BIT_C);`
* instead of `bool isBSet = (bits & (1<<BIT_B)) != 0;` you could test with `bool isBSet = bits.IsBitSet(BIT_C);`
* Declares many other convenient methods like [SetBit()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1BitChord.html#a707c124be95264b488524bfbbb2ac38c), [ClearBit()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1BitChord.html#a89120ee9efe2841f0b0a82bbc510737e), [ToggleBit()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1BitChord.html#ad9b3774358f5e918e2fbd060cc8cb082), [AreAnyBitsSet()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1BitChord.html#aa2720a7f8e6519e5ba35c2751de65b90), etc.
* Methods and constructors using variadic arguments work under both C++11 (using C++11 variadic templates) and C++03 (using a fallback macro-based implementation)
