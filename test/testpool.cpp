/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "message/Message.h"
#include "system/SetupSystem.h"
#include "util/TimeUtilityFunctions.h"  // for Snooze64()

using namespace muscle;

// This program tests the relative speeds of various object allocation strategies.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;  // required!

   const uint32 NUM_OBJECTS = 10000000;
   Queue<MessageRef> tempQ;
   if (tempQ.EnsureSize(NUM_OBJECTS, true).IsError()) return 10;

   Message * allocedArray = NULL;
   const int whichTest = (argc>1) ? atoi(argv[1]) : -1;
   const uint64 startTime = GetRunTime64();
   switch(whichTest)
   {
      case 1:
      {
         // See how long it takes just to allocate an array of objects
         allocedArray = new Message[NUM_OBJECTS];
      }
      break;

      case 2:
      {
         // As above, but with deletion also
         delete [] new Message[NUM_OBJECTS];
      }
      break;

      case 3:
      {
         // See how long it takes to allocate each object individually
         for (uint32 i=0; i<NUM_OBJECTS; i++) tempQ[i].SetRef(new Message);
      }
      break;

      case 4:
      {
         // As above, but we delete the item after allocating it
         for (uint32 i=0; i<NUM_OBJECTS; i++) tempQ[i].SetRef(new Message);
         for (uint32 i=0; i<NUM_OBJECTS; i++) tempQ[i].Reset();
      }
      break;

      case 5:
      {
         // See how long it takes to allocate each object individually
         for (uint32 i=0; i<NUM_OBJECTS; i++) tempQ[i] = GetMessageFromPool();
      }
      break;

      case 6:
      {
         // As above, but then we clear the queue
         for (uint32 i=0; i<NUM_OBJECTS; i++) tempQ[i] = GetMessageFromPool();
         tempQ.Clear();
      }
      break;

      case 7:
      {
         // As above, but we only use one object at a time
         for (uint32 i=0; i<NUM_OBJECTS; i++) (void) GetMessageFromPool();
      }
      break;

      default:
         printf("Usage:  testpools <testnum>   (where testnum is between 1 and 7)\n");
      break;
   }

   const uint64 endTime = GetRunTime64();
   printf("Test duration for " UINT32_FORMAT_SPEC " objects was " UINT64_FORMAT_SPEC "ms \n", NUM_OBJECTS, (endTime-startTime)/1000);
   delete [] allocedArray;

   if ((argc > 2)&&(strcmp(argv[2], "hold") == 0))
   {
      printf("Holding indefinitely, so that you can look at OS reported memory usage...\n");
      while(1) Snooze64(SecondsToMicros(10));
   }

   return 0;
}
