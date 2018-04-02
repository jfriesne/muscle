#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/AtomicCounter.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This little program demonstrates basic usage of the muscle::AtomicCounter class.\n");
   printf("\n");
   printf("Note that this program calls AtomicCounter::GetCount() just to show what is going\n");
   printf("on with the counter's value -- that's okay because this example uses only a single\n");
   printf("thread, but in the more usual multi-threaded context, it's better to not call\n");
   printf("AtomicCounter::GetCount() if you can avoid it, since the value you get back may\n");
   printf("be obsolete (due to race conditions) by the time you examine it.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   AtomicCounter ac;
   printf("AtomicCounter's initial value is %i\n", ac.GetCount());

   const int repetitions = 5;

   for (int i=0; i<repetitions; i++)
   {
      const bool ret = ac.AtomicIncrement();
      printf("After ac.AtomicIncrement() was called and returned %i, the atomic counter's new value is %i\n", ret, ac.GetCount());
   }

   for (int i=0; i<repetitions; i++)
   {
      const bool ret = ac.AtomicDecrement();
      printf("After ac.AtomicDecrement() was called and returned %i, the atomic counter's new value is %i\n", ret, ac.GetCount());
   }

   return 0;
}
