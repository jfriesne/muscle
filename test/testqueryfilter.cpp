/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "regex/QueryFilter.h"
#include "util/String.h"

using namespace muscle;

/** Dummy QueryFilter, just so I can test the various "Gate" filters easily */
class DummyQueryFilter : public QueryFilter
{
public:
   explicit DummyQueryFilter(bool ret) : _ret(ret) {/* empty */}

   virtual uint32 TypeCode() const {return 0;}

   virtual bool Matches(ConstMessageRef &, const DataNode *) const {return _ret;}

private:
   const bool _ret;
};

// Generates a truth-table for all inputs to the QueryFilter, just so I can eyeball-check that it does the right thing
static void TestQueryFilter(MultiQueryFilter & qf, const char * desc, const char * instructions, uint32 max)
{
   printf("------------------------- %s ---------------------------\n", desc);

   uint32 numStates = 1;
   for (uint32 numArgs=0; numArgs<6; numArgs++)
   {
      printf("\n%s with " UINT32_FORMAT_SPEC " ARGS (%s)\n", desc, numArgs, String((numArgs>0)?instructions:"Degenerate case").Arg(muscleMin(numArgs-1, max))());

      for (uint32 state=0; state<numStates; state++)
      {
         qf.GetChildren().Clear();

         String inputs;
         for (uint32 a=0; a<numArgs; a++)
         {
            const bool isChildTrue = ((state & (1<<a)) != 0);
            (void) qf.GetChildren().AddTail(ConstQueryFilterRef(new DummyQueryFilter(isChildTrue)));
            inputs = inputs.Prepend("%1 ").Arg((int)isChildTrue);
         }
         ConstMessageRef dummyMsg;
         printf(" %s--> %i\n", inputs(), qf.Matches(dummyMsg, NULL));
      }
      
      numStates *= 2;
   } 
   printf("\n");
}

// This program exercises some of the QueryFilter classes.
int main(void) 
{
   {OrQueryFilter   qf; TestQueryFilter(qf, "OR",   "return true iff at least one child returns true", 0);}
   {AndQueryFilter  qf; TestQueryFilter(qf, "AND",  "return true iff all children return true", 0);}
   {NorQueryFilter  qf; TestQueryFilter(qf, "NOR",  "return true iff no children return true", 0);}
   {NandQueryFilter qf; TestQueryFilter(qf, "NAND", "return true unless all children return true", 0);}
   {XorQueryFilter  qf; TestQueryFilter(qf, "XOR",  "return true iff an odd number of children return true", 0);}

   {MinimumThresholdQueryFilter qf(2); TestQueryFilter(qf, "MIN2", "return true iff more than %1 children return true", 2);}
   {MinimumThresholdQueryFilter qf(3); TestQueryFilter(qf, "MIN3", "return true iff more than %1 children return true", 3);}

   {MaximumThresholdQueryFilter qf(2); TestQueryFilter(qf, "MAX2", "return true iff no more than %1 children return true", 2);}
   {MaximumThresholdQueryFilter qf(3); TestQueryFilter(qf, "MAX3", "return true iff no more than %1 children return true", 3);}
   return 0;
}
