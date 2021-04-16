/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>
#include <time.h>

#include "support/MuscleSupport.h"

using namespace muscle;

status_t TestFunction()
{
   return ((rand()%2)==0) 
          ? B_NO_ERROR 
          : B_ERROR("Bad luck");
}

status_t Func1() 
{
   printf("Called Func1(), returning OK\n");
   return B_NO_ERROR;
}

status_t Func2() 
{
   printf("Called Func2(), returning Error\n");
   return B_ERROR("Func2");
}

status_t Func3() 
{
   printf("Called Func3(), returning Error\n");
   return B_ERROR("Func3");
}

// This program exercises the String class.
int main(void) 
{
   srand((unsigned)time(NULL));

   // Simple test
   {
      status_t ret = TestFunction();
      if (ret.IsOK()) printf("Success!  [%s]\n", ret());
                 else printf("Failure:  [%s]\n", ret());
      printf("\n");
   }

   // Test short-circuit && with string-error-message
   {
      status_t ret;
      if ((Func1().IsOK(ret))   // should succeed
        &&(Func2().IsOK(ret))   // should fail
        &&(Func3().IsOK(ret)))  // should not be called
      {
         printf("A All functions succeeded!\n");
      }
      else printf("A Func failure: [%s]\n", ret());

      printf("\n");
   }

   // Test short-circuit || with string-error-message
   {
      status_t ret;
      if ((Func1().IsError(ret))  // should succeed
        ||(Func2().IsError(ret))  // should fail
        ||(Func3().IsError(ret))) // should not be called
      {
         printf("B Func failure: [%s]\n", ret());
      }
      else printf("B All functions succeeded!\n");
   }

#ifdef MUSCLE_USE_CPLUSPLUS17
   // Test And-chaining
   {
      printf("Testing And-Chaining:\n");
      status_t ret = Func1()
                .And(Func2())
                .And(Func3());
      printf("Final result is [%s]\n", ret());
   }
#endif

   return 0;
}
