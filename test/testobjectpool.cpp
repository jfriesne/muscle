/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/SharedMemory.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for GetInsecurePseudoRandomNumber()
#include "util/NetworkUtilityFunctions.h"
#include "util/ObjectPool.h"
#include "util/RefCount.h"

using namespace muscle;

static uint32 _counter = 0;
static uint32 _maxCount = 0;

class Counter : public RefCountable
{
public:
   Counter()
   {
      _counter++;
      //printf("++" UINT32_FORMAT_SPEC "\n", _counter);
      if (_counter > _maxCount)
      {
         _maxCount = _counter;
         printf("MaxObjectCount is now " UINT32_FORMAT_SPEC "\n", _maxCount);
      }
   }

   ~Counter()
   {
      _counter--;
      //printf("--" UINT32_FORMAT_SPEC "\n", _counter);
   }
};
DECLARE_REFTYPES(Counter);

ObjectPool<Counter> _pool;
static CounterRef GetCounterRefFromPool() {return CounterRef(_pool.ObtainObject());}

// This program tests the ObjectPool class to see how well it manages memory usage
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   const bool isFromScript = ((argc >= 2)&&(strcmp(argv[1], "fromscript") == 0));
   const bool interactive  = ((!isFromScript)&&((argc < 2)||(strcmp(argv[1], "y") != 0)));  // ./testobjectpool y  <-- turbo mode

   enum {MAX_NUM_REFS = 10000};
   CounterRef refs[MAX_NUM_REFS];

   int count = isFromScript ? 10000 : -1;
   while(count != 0)
   {
      const uint32 max = GetInsecurePseudoRandomNumber(10)+1;
      for (uint32 i=0; i<MAX_NUM_REFS; i++)
      {
         const uint32 idx = GetInsecurePseudoRandomNumber(MAX_NUM_REFS);
         if (GetInsecurePseudoRandomNumber(max)==0) refs[idx] = GetCounterRefFromPool();
                                               else refs[idx].Reset();
      }

      AbstractObjectManager::GlobalPerformSanityCheck();
      AbstractObjectManager::GlobalPrintRecyclersToStream(stdout);

      printf("(max=" UINT32_FORMAT_SPEC ") Continue? y/n\n", max);
      char buf[64] = "";
      if ((interactive)&&(fgets(buf, sizeof(buf), stdin)))
      {
              if (buf[0] == 'n') break;
         else if (buf[0] == 'c')
         {
            for (uint32 i=0; i<MAX_NUM_REFS; i++) refs[i].Reset();
            AbstractObjectManager::GlobalPrintRecyclersToStream(stdout);
            AbstractObjectManager::GlobalPerformSanityCheck();
         }
      }

      if (count > 0) count--;
   }

   return 0;
}
