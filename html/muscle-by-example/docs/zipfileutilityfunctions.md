# muscle::ZipFileUtilityFunctions class [(API)](https://public.msli.com/lcs/muscle/html/group__zipfileutilityfunctions.html)

```#include "zlib/ZipFileUtilityFunctions.h"```

* The functions in `ZipFileUtilityFunctions.h` allow you to easily read and write small .zip files
* `ReadZipFile()` reads in a .zip file, inflates its contents, and returns the result as a `Message` object. 
* In the returned `Message`, sub-directories are represented as sub-Messages, and files are represented as B_RAW_DATA_TYPE fields with the filename as the field-name.
* `WriteZipFile()` takes a `Message` object (in the form returned by `ReadZipFile()` and writes out the corresponding .zip file
* Note that since the entire .zip file is read into memory at once (and inflated!), huge .zip files probably won't work well with this API.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/zipfileutilityfunctions` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [zipfileutilityfunctions/example_1_read_zip.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zipfileutilityfunctions/example_1_read_zip.cpp)
* [zipfileutilityfunctions/example_2_write_zip.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/zipfileutilityfunctions/example_2_write_zip.cpp)
