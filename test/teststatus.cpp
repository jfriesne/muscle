/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <time.h>
#include "support/MuscleSupport.h"

using namespace muscle;

status_t TestFunction()
{
   return ((rand()%2)==0) 
          ? B_NO_ERROR 
          : B_ERROR("Bad luck");
}

// This program exercises the String class.
int main(void) 
{
   srand(time(NULL));

   status_t ret = TestFunction();
   if (ret == B_NO_ERROR) printf("Success!  [%s]\n", ret());
                     else printf("Failure:  [%s]\n", ret());
   return 0;
}
