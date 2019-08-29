In this directory you will find Visual C++ project files created by Mika

Note that these project files require Visual C++ 2019.

To compile muscle, do the following:

1. Open the "muscle.sln" solution file in VC++

2. Select correct architecture, choices are x86, x64, ARM and ARM64, and Debug or Release build. Usually you would want x86 and Release build. ARM and ARM64 versions require Windows 10 SDK.

3. Click Build->Build Solution

4. Sit back and relax.  The muscle build should complete in a minute or two.

5. Open the correct architecture directory and build type directory that are created inside vc++141. You should see three files there: admin.exe, muscled.exe, and muscle.lib. You can run your server with muscled.exe, and maintain it with admin.exe. You can build your own apps and link them against muscle.lib.

-Mika Lindqvist (aka "Monni")
