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

CMake can be used to compile all of these examples under any OS, e.g.:

```
   cd muscle
   cmake -B _build -S .
   cmake --build _build --parallel 8
```

When the compilation finishes, the binaries of the muscle-by-example programs will be found
in the `muscle/_build/html/muscle-by-example/examples/*` sub-folders.

MacOS/X and Linux users who don't want to deal with cmake can alternatively just run `make -j8`
inside this examples folder, and make will compile all of the example programs in this folder.
(if you get errors while compiling gzlib.c, you may need to run `cd muscle/zlib/zlib; ./configure` first)
