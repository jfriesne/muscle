#!/bin/bash

function RunTest {
   testName="$1"
   echo ""
   cmdLine="$testName fromscript"
   echo "Running test:" $cmdLine
   ./$testName fromscript > tests_output/$testName.out.txt
   result=$?
   if [[ $result != 0 ]]
   then
      echo "Test result:  FAILED, exit code: " $result
   else
      echo "Test result:  passed"
   fi

   return $result
}

echo "Compiling test suite..."
make -j8

# Generate a list of test executables to run
find . -name 'test*' -type f -maxdepth 1 -print | grep -v '..*\.' > testfiles.txt
sort <testfiles.txt > sortedtextfiles.txt
mv sortedtextfiles.txt textfiles.txt
allTests=`cat textfiles.txt` # Read the list of executable names into an array
rm -f textfiles.txt

rm -rf tests_output
mkdir tests_output

echo "Running regression test suite..."

PASSCOUNT=0
FAILCOUNT=0
FAILOUTPUT=""

for testName in $allTests
do
   RunTest $testName
   result=$?

   if [[ $result != 0 ]]
   then
      FAILCOUNT=$((FAILCOUNT+1))
      FAILOUTPUT="$FAILOUTPUT tests/output/$testName.out.txt"
   else
      PASSCOUNT=$((PASSCOUNT+1))
   fi
done

echo ""
echo ""
echo ""
echo "Test run complete:"
echo "  PASS COUNT: " $PASSCOUNT
echo "  FAIL COUNT: " $FAILCOUNT

if [[ $FAILCOUNT == 0 ]]
then
   echo "All tests passed, returning code 0"
   exit 0
else
   echo "One or more tests FAILED, returning code 10"
   echo "Review these files for details:  " $FAILOUTPUT
   exit 10
fi
