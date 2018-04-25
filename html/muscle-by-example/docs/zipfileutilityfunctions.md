# muscle::ZipFileUtilityFunctions class [(API)](https://public.msli.com/lcs/muscle/html/group__zipfileutilityfunctions.html)

```#include "zlib/ZipFileUtilityFunctions.h"```

The functions in [ZipFileUtilityFunctions](https://public.msli.com/lcs/muscle/html/group__zipfileutilityfunctions.html) allow you to easily read and write small .zip files.

* [ReadZipFile()](https://public.msli.com/lcs/muscle/html/group__zipfileutilityfunctions.html#ga1cbe2b5f97bd7fe3e10aa8448f426b73) reads in a .zip file, inflates its contents, and returns the result as a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object. 
* In the returned [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html), sub-directories are represented as sub-Messages, and files are represented as B_RAW_DATA_TYPE fields with the filename as the field-name.
* [WriteZipFile()](https://public.msli.com/lcs/muscle/html/group__zipfileutilityfunctions.html#gad3bfc57e3e0bd8a190cc00d174b2efc9) takes a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object (in the form returned by [ReadZipFile()](https://public.msli.com/lcs/muscle/html/group__zipfileutilityfunctions.html#ga1cbe2b5f97bd7fe3e10aa8448f426b73) and writes out the corresponding .zip file
* Note that since the entire .zip file is read into memory at once (and inflated!), reading huge .zip files via this API will require a lot of RAM.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/zipfileutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [zipfileutilityfunctions/example_1_read_zip.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zipfileutilityfunctions/example_1_read_zip.cpp)
* [zipfileutilityfunctions/example_2_write_zip.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zipfileutilityfunctions/example_2_write_zip.cpp)
