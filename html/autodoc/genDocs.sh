#!/bin/sh
echo "Generating doxygen documentation.  "
echo "You will need to have doxygen installed in"
echo "order for this script to work."
echo ""
export MUSCLE_VERSION=$(grep MUSCLE_VERSION_STRING ../../support/MuscleSupport.h  | cut -c 32-35)
doxygen muscle.dox
echo "Done!"
