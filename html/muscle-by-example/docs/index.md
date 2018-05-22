# MUSCLE by Example

For full Doxygen AutoDocs and other documentation visit [the MUSCLE web page](https://public.msli.com/lcs/muscle/)

This tour is meant to introduce the reader to the various MUSCLE APIs through simple
"toy" example programs that demonstrate each class's purpose and common usage-patterns.

The examples are ordered in "bottom-up" fashion, with the simplest and lowest-level
APIs demonstrated first, followed by higher-level APIs that use those first APIs,
and so on until we reach the highest-level APIs near the end of the tour.

While reading the the HTML topics (starting at the top), you can also have a Terminal
window open to compile and run the corresponding toy example programs.  To do that,
cd into the muscle/html/muscle-by-example/examples folder and enter "make" to
compile the programs, then you can run any of them from the command line 
(e.g. "cd string ; ./example_1_basic_usage").

The MUSCLE toy programs can be compiled and run under any common OS.

For Linux and MacOS/X, they can be compiled by typing "make" in the examples
sub-directory (or in any sub-directory underneath it).

For Windows, use Visual Studio 2015 or higher and run the BUILD_ALL.bat
file in the examples/vc++14 sub-directory.
