In this directory you will find Visual C++ project files created by Vitaliy

Note that MUSCLE 5.xx and higher won't compile under VisualC++6, so you will need
Visual Studio 2008 or better (Visual Studio 2008 Express will work too).

To compile muscle, do the following:

1. Open the "muscle.vcproj" project file in VC++

2. Click Build->Build Solution

3. Sit back and relax.  The muscle build should complete in a minute or two.

4. Open the new "Build" folder that is created inside vc++.  You should see
   three files there: admin.exe, muscled.exe, and muscle.lib. You can run
   your server with muscled.exe, and maintain it with admin.exe.  You can
   build your own apps and link them against muscle.lib.

-Vitaliy Mikitchenko (aka "VitViper")
