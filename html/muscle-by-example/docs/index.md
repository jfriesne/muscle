# MUSCLE by Example

For the latest MUSCLE source code, full Doxygen AutoDocs, and other MUSCLE documentation, visit [the MUSCLE web page](https://public.msli.com/lcs/muscle/).

This tour is meant to introduce the reader to the various MUSCLE APIs through simple
"toy" example programs that demonstrate each class's purpose and common usage-patterns.

The examples are ordered in "bottom-up" fashion, with the simplest and lowest-level
APIs demonstrated first, followed by some higher-level APIs that make use of those first APIs,
and so on until we reach the highest-level APIs near the end of the tour.

While reading the HTML topics pages (starting at the top), you can also have a Terminal
window open, so you can view, compile, and execute the corresponding toy example programs.

The MUSCLE toy example programs may be compiled and run under any common desktop OS.

Under Linux and MacOS/X, you can compile them by `cd`'ing to the 
`muscle/html/muscle-by-example/examples` sub-directory (or to any one
of the sub-directories underneath it) and typing `make`.

Under Windows, make sure Visual Studio 2015 or higher, is installed, 
and run the `BUILD_ALL.bat` file that is located in the 
`muscle\html\muscle-by-example\examples\vc++14` sub-directory (or, 
if you prefer to use an IDE, you can double-click on any of the 
`.vcxproj` or `.sln` files in that same folder to launch Visual Studio)
