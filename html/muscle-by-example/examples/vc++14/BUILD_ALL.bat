echo This script will build muscle.lib, and then build all
echo of the "MUSCLE by Example" .exe files that use muscle.lib.
echo ""
echo Note that this script assumes you have Visual Studio 2015 
echo or later installed.
echo ""

cd ..\..\..\..\vc++14
devenv muscle.sln /Upgrade
devenv muscle.sln /build Debug /project muscle

cd ..\html\muscle-by-example\examples\vc++14\MuscleByExample 
devenv MuscleByExample.sln /Upgrade 
devenv MuscleByExample.sln /build 

echo ""
echo If all went well, all the example .exe files should now
echo be in the various directories where their corresponding 
echo .cpp files are located.
echo ""
