/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "system/SetupSystem.h"
#include "util/Queue.h"

using namespace muscle;

// This program prints out a series of hash-code calculatations for a series of known, arbitrary byte
// sequences.  The intent is just to check that our hash-code functions give the same results on different
// CPU architectures.
int main(void) 
{
   srand(0);  // deliberately passing in a fixed seed value as we want our random sequence to always be the same

   const uint32 MAX_BUF_SIZE = 1000;

   Queue<uint8> bytes;
   if (bytes.EnsureSize(MAX_BUF_SIZE) != B_NO_ERROR) return 10;
   
   uint32 metaHash32 = 0;
   uint64 metaHash64 = 0;
   for (uint32 i=0; i<MAX_BUF_SIZE; i++)
   {
      (void) bytes.AddTail((uint8)(rand()%256));  // guaranteed not to fail
      const uint32 qLen   = bytes.GetNumItems();
      const uint32 hash32 = CalculateHashCode(  bytes.HeadPointer(), qLen);
      const uint64 hash64 = CalculateHashCode64(bytes.HeadPointer(), qLen);
      printf("len=" UINT32_FORMAT_SPEC " hash=" UINT32_FORMAT_SPEC " hash64=" UINT64_FORMAT_SPEC "\n", qLen, hash32, hash64);
      metaHash32 += hash32;
      metaHash64 += hash64;
   }
   printf("For " UINT32_FORMAT_SPEC " items, metaHash32=" UINT32_FORMAT_SPEC ", metaHash64=" UINT64_FORMAT_SPEC "\n", bytes.GetNumItems(), metaHash32, metaHash64);

   return 0;
}
