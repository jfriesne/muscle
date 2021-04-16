/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "util/String.h"
#include "util/StringTokenizer.h"

// This program simply reads output from hexterm on stdin,
// and prints out the corresponding hex-ASCII bytes (without
// the meta-data content) on stdout.  I wrote it because I
// sometimes want to try resending the hex strings that hexterm
// previously received, and reformatting it by hand is tedious.

#ifdef EXAMPLE_EXPECTED_INPUT
[I 07/17 14:19:42] [PTZS] --- Received from [::192.168.71.85]:63787 (<1 millisecond since prev) (143 bytes):
[I 07/17 14:19:42] [PTTG] 0000: ATC4............ [41 54 43 34 00 02 00 00 00 01 00 00 8f ff ff ff]
[I 07/17 14:19:42] [PTTG] 0016: ....}.0.....d.X. [ff 01 01 00 7d 01 30 04 02 00 1d 00 64 c0 58 bd]
[I 07/17 14:19:42] [PTTG] 0032: O............... [4f e0 00 00 00 00 00 00 00 00 00 00 00 80 00 00]
[I 07/17 14:19:42] [PTTG] 0048: .........d.X.O.. [00 00 00 00 00 06 00 1e 00 64 c0 58 bd 4f e0 00]
[I 07/17 14:19:42] [PTTG] 0064: ................ [00 00 00 00 00 00 00 00 00 00 80 00 00 00 00 00]
[I 07/17 14:19:42] [PTTG] 0080: .......d.X.O.... [00 00 00 06 00 1e 00 64 c0 58 bd 4f e0 00 00 00]
[I 07/17 14:19:42] [PTTG] 0096: ................ [00 00 00 00 00 00 00 00 80 00 00 00 00 00 00 00]
[I 07/17 14:19:42] [PTTG] 0112: .....d.X.O...... [01 06 00 1e 00 64 c0 58 bd 4f e0 00 00 00 00 00]
[I 07/17 14:19:42] [PTTG] 0128: ...............  [00 00 00 00 00 00 80 00 00 00 00 00 00 00 02]
[I 07/17 14:19:42] [PTZS] --- Received from [::192.168.71.85]:63787 (33 milliseconds since prev) (143 bytes):
[I 07/17 14:19:42] [PTTG] 0000: ATC4............ [41 54 43 34 00 02 00 00 00 01 00 00 8f ff ff ff]
[I 07/17 14:19:42] [PTTG] 0016: ....}.0.....d.V/ [ff 01 01 00 7d 01 30 04 02 00 1d 00 64 c0 56 22]
[I 07/17 14:19:42] [PTTG] 0032: . .............. [b0 20 00 00 00 00 00 00 00 00 00 00 00 80 00 00]
[I 07/17 14:19:42] [PTTG] 0048: .........d.V/. . [00 00 00 00 00 06 00 1e 00 64 c0 56 22 b0 20 00]
[I 07/17 14:19:42] [PTTG] 0064: ................ [00 00 00 00 00 00 00 00 00 00 80 00 00 00 00 00]
[I 07/17 14:19:42] [PTTG] 0080: .......d.V/. ... [00 00 00 06 00 1e 00 64 c0 56 22 b0 20 00 00 00]
[I 07/17 14:19:42] [PTTG] 0096: ................ [00 00 00 00 00 00 00 00 80 00 00 00 00 00 00 00]
[I 07/17 14:19:42] [PTTG] 0112: .....d.V/. ..... [01 06 00 1e 00 64 c0 56 22 b0 20 00 00 00 00 00]
[I 07/17 14:19:42] [PTTG] 0128: ...............  [00 00 00 00 00 00 80 00 00 00 00 00 00 00 02]
#endif

using namespace muscle;

int main(int, char **) 
{
   bool printedSep = true;
   char buf[1024];
   while(fgets(buf, sizeof(buf), stdin))
   {
      String s = buf; s = s.Trim();
      const int32 leftBracket  = s.LastIndexOf('[');
      const int32 rightBracket = s.LastIndexOf(']');
      if ((leftBracket >= 0)&&(rightBracket >= 0)&&(((uint32)(rightBracket+1)) == s.Length()))
      {
         const String hexBytes = s.Substring(leftBracket+1, rightBracket);

         // Let's verify that all of the hexBytes are of the form XX
         bool sawWeirdness = false;
         {
            StringTokenizer tok(hexBytes(), NULL, " ");
            const char * t;
            while(((t = tok()) != NULL)&&(sawWeirdness == false))
            {
               if (strlen(t) != 2) {sawWeirdness = true; break;}
               for (int i=0; i<2; i++)
               {
                  char c = t[i];
                  if ((muscleInRange(c, '0', '9') == false)&&(muscleInRange(c, 'a', 'f') == false)&&(muscleInRange(c, 'A', 'F') == false))
                  {
                     sawWeirdness = true;
                     break;
                  }
               }
            }
         }

         if (sawWeirdness == false)
         {
            printf("%s ", hexBytes());
            printedSep = false;
         }
      }
      else if (printedSep == false)
      {
         printf("\n\n");
         printedSep = true;
      }
   }
   return 0;
}
