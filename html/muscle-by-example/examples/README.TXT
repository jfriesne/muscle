This folder contains the "toy example" programs for the "MUSCLE by Example" self-guided tour.

CMake can be used to compile all of these examples under any OS, e.g.:

   cd muscle
   cmake -B _build -S .
   cmake --build _build --parallel 8

When the compilation finishes, the binaries of the muscle-by-example programs will be found
in the muscle/_build/html/muscle-by-example/examples/* sub-folders.

MacOS/X and Linux users who don't want to deal with cmake can alternatively just run "make -j8"
inside this examples folder, and make will compile all of the example programs in this folder.

-Jeremy
