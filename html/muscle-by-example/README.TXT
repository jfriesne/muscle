This is the source directory for the "MUSCLE by Example" self-guided tour of the MUSCLE APIs.

This tour is meant to introduce people to the various MUSCLE APIs by showing them simple
"toy" example programs demonstrating each class's purpose and usage-patterns.

The examples are ordered in "bottom-up" fashion, with the simplest and lowest-level
APIs demonstrated first, followed by higher-level APIs that might use the first APIs,
and so on until we reach the highest-level APIs at the end of the tour.

The recommended way to take the tour is by reading this documentation as HTML in
a web browser.  The online HTML can be found here:

   https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/site/index.html

While reading the the HTML topics (starting at the top), you can also have a Terminal
window open to compile and run the corresponding toy example programs.  To do that,
cd into the muscle/html/muscle-by-example/examples/* folders and enter "make" to
compile the programs, then run them from the command line (e.g. "./example_1_basic_usage",
etc).

The MUSCLE toy programs can be compiled under any popular OS.  The Makefiles that
are included to compile them will work under Linux and MacOS/X (and possibly
other POSIX-y OS's too) -- if you want to compile under Windows, use Visual
Studio 2015 (or later) and the project files in the examples\vc++14
sub-directory, instead.

If you'd like to generate your own HTML from the Markdown files in the "docs" folder,
you can do so by installing the MkDocs python package (per the instructions at
http://www.mkdocs.org/#installation ), then cd'ing into the docs folder and entering
either this command:

    mkdocs build

... which will create new sub-folder called 'site' for you to browse, or
by entering this command:

    mkdocs serve

... and pointing your web browser to http://127.0.0.1:8000, at which point
you can read the HTML as served up "live" by the mkdocs program's built-in
web server.

Reading the documentation markdown directly as ASCII text is also an
option, if you're into that kind of thing.

Direct any questions to:  jfriesne@gmail.com

-Jeremy Friesner
3/27/2018
