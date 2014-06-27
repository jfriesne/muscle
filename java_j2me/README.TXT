This folder contains Bryan Varner's port of the MUSCLE Java API to J2ME 
(Java 2 Micro Edition).  This code is designed to run on cell phones and the 
like, where the Java 1.4.x APIs are not available.  If you are developing for 
personal computer platforms that use J2SE (Java 2 Standard Edition), you 
probably want to use the Java code found in the muscle/java folder instead
of the code in this folder.

-Jeremy 10/17/2007

---------------------------------------------------------

To build you'll need to install the WTK 2.0 (or newer) from Sun.
		http://java.sun.com/products/sjwtoolkit/overview.html#3
Once that is installed, update the wtk.home, and device.profile in build.properties.

I've tested the build.xml with Apache Ant 1.7 on windows XP.

We're using the Antenna (http://antenna.sourceforge.net) 1.0 tasks for Ant.
You'll need to download antenna-bin-1.0.0.jar from http://antenna.sourceforge.net and
place in the muscle/java_j2me/support folder.  (it's too big to include in the MUSCLE 
distribution, sorry!)

The build.xml file has the muscle.jar and the testclient.jar segmented clearly 
to demonstrate how to include the muscle.jar in a j2me application as an 
external library. It might be wise to use this as a sample when starting new 
mobile projects.

-Bryan 10/18/2007
