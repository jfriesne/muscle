This folder contains the "toy example" programs for the "MUSCLE by Example" self-guided tour.

MacOS/X and Linux users:

To compile the examples, you can either compile them all at once by typing "make" while in this 
folder, or you can cd to the individual sub-directory you are interested in and enter "make" from 
there, if you'd prefer just to compile the examples in that directory.

Windows users:

Assuming you have Visual Studio 2015 (or later) installed, you can compile
the all of the examples from the DOS prompt by cd'ing into the vc++14 
sub-directory and entering this command:

   BUILD_ALL.bat

... or alternatively you can open VisualStudio and compile the following
.sln files (in the listed order!) from the GUI:

   muscle\vc++14\muscle.sln
   muscle\html\muscle-by-example\examples\vc++14\MuscleByExample.sln

-Jeremy
