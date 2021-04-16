/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/SetupSystem.h"
#include "util/Queue.h"

using namespace muscle;

// A built-in pseudo-random-number generator, just so that we can guarantee our random
// numbers will always be the same on all platforms (and thereby avoid false-positives
// if rand() on platform X gives different values than rand() on platform Y)
// Stolen directly from Wikipedia:  https://en.wikipedia.org/wiki/Lehmer_random_number_generator
uint32 lcg_parkmiller()
{
   static uint32 _state = 66;

   _state = (((uint64)_state)*48271) % 0x7fffffff;
   return _state;
}

// This program prints out a series of hash-code calculatations for a series of known, arbitrary byte
// sequences.  The intent is just to check that our hash-code functions give the same results on different
// CPU architectures.
int main(void) 
{
   const uint32 MAX_BUF_SIZE = 1000;

   Queue<uint8> bytes;
   if (bytes.EnsureSize(MAX_BUF_SIZE).IsError()) return 10;
   
   uint32 metaHash32 = 0;
   uint64 metaHash64 = 0;
   for (uint32 i=0; i<MAX_BUF_SIZE; i++)
   {
      (void) bytes.AddTail((uint8) (lcg_parkmiller() & 0xFF));
      //printf("i=%u lastByte=%u\n", i, bytes.Tail());

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
