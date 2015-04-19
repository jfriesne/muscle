/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>
#include "support/MuscleSupport.h"

// Just a silly program to print the alphabetical equivalent
// of the passed in integer value.
int main(int argc, char ** argv)
{
   if ((argc == 2)&&(muscleInRange(argv[1][0], '0', '9')))
   {
      char * code = argv[1];
      uint32 val = strtod(code, NULL);

      for (int i=3; i>=0; i--)
      {
         char c = (char) ((val>>(i*8))&0xFF);
         putc(c, stdout);
      }
      printf("\n");
   }
   else printf("Usage Example:  printtypecode (integer_value)\n");
   return 0;
}
