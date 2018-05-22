echo This script will build muscle.lib, and then build all
echo of the "MUSCLE by Example" .exe files that use muscle.lib.
echo ""
echo Note that this script assumes you have Visual Studio 2015 
echo or later installed.
echo ""

cd ..\..\..\..\vc++14
devenv muscle.sln /upgrade
devenv muscle.sln /Build "Debug|Win32" /Project muscle

cd ..\html\muscle-by-example\examples\vc++14
devenv MuscleByExample.sln /Upgrade 
devenv MuscleByExample.sln /Build "Debug|x86"

echo ""
echo If all went well, all the example .exe files should now
echo be in the various directories where their corresponding 
echo .cpp files are located.
echo ""
