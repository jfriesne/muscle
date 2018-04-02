# MUSCLE by Example

For full DOxygen AutoDocs and other documentation visit [the MUSCLE web page](https://public.msli.com/lcs/muscle/)

This tour is meant to introduce people to the various MUSCLE APIs by showing them simple
"toy" example programs demonstrating each class's purpose and usage-patterns.

The examples are ordered in "bottom-up" fashion, with the simplest and lowest-level
APIs demonstrated first, followed by higher-level APIs that might use the first APIs,
and so on until we reach the highest-level APIs at the end of the tour.

While reading the the HTML topics (starting at the top), you can also have a Terminal
window open to compile and run the corresponding toy example programs.  To do that,
cd into the muscle/html/muscle-by-example/examples/* folders and enter "make" to
compile the programs, then run them from the command line (e.g. "./example_1_basic_usage",
etc).

Note that while the MUSCLE toy programs can be compiled and run under any OS, the Makefiles that
are included to compile them will work only under Linux and MacOS/X, for now -- Windows
users are on their own for the time being.
