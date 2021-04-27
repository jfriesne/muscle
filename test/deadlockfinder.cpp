/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "util/Hashtable.h"
#include "util/Queue.h"
#include "util/String.h"
#include "util/StringTokenizer.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"

// This program is useful for finding potential synchronization deadlocks in your multi-threaded
// application.  To use it add -DMUSCLE_ENABLE_DEADLOCK_FINDER to your Makefile and then fully
// recompile your program.
//
// Then run your program and have it output stdout to a file, like this;
//
// ./mymultithreadedprogram > outfile
//
// Once you have exercised your program in the normal manner, run deadlockfinder like this:
//
// ./deadlockfinder <outfile
//
// where outfile is the output file your program generated to stdout.  Then deadlockfinder will
// parse through the output and build up a catalog of all the locking sequences that were used.
// When it is done, it will print out all the unique multi-lock locking sequences, and you can then go
// through them and make sure that all the locks were always locked in a well-defined order.
// For example, if you saw output like this:
//
// SEQUENCE 25/50:
//     0.  LockA & 0x102ec580 & MyFile.cpp:239
//     1.  LockB & 0x101b0dfc & AnotherFile.cpp:141
//
// [...]
//
// SEQUENCE 37/50:
//     0.  LockB & 0x102ec580 & MyFile.cpp:239
//     1.  LockA & 0x101b0dfc & AnotherFile.cpp:141
//
// That would indicate a potential deadlock condition, since the two locks were not
// always being locked in the same order.  (i.e. thread 1 might lock LockA, then thread 2
// might lock LockB, and then thread 1 would try to lock LockB and block forever, while
// thread 2 would try to lock LockA and block forever... the result being that some or
// all of your program would hang.

using namespace muscle;

#ifdef NOT_CURRENTLY_USED
static void PrintState(const Hashtable<int, Queue<String> > & state)
{
   printf("--------- Begin Current state ------------\n");
   for (HashtableIterator<int, Queue<String> > iter(state); iter.HasData(); iter++)
   {
      printf("  Thread %i:\n", iter.GetKey());
      for (uint32 i=0; i<iter.GetValue().GetNumItems(); i++) printf("    " UINT32_FORMAT_SPEC ". %s\n", i, iter.GetValue()[i]());
   }
   printf("--------- End Current state ------------\n");
}
#endif

static bool LogsMatchExceptForLocation(const Queue<String> & q1, const Queue<String> & q2)
{
   if (q1.GetNumItems() != q2.GetNumItems()) return false;

   for (uint32 i=0; i<q1.GetNumItems(); i++)
   {
      String s1 = q1[i]; {int32 lastAmp = s1.LastIndexOf('&'); s1 = s1.Substring(0, lastAmp);}
      String s2 = q2[i]; {int32 lastAmp = s2.LastIndexOf('&'); s2 = s2.Substring(0, lastAmp);}
      if (s1 != s2) return false;
   }
   return true;
}

static String ExtractPointerString(const String & key) {return key.Substring("p=").Substring(0, " ");}
static status_t CheckOrderingConstraints(const String & key, const Queue<String> & seqB, const Queue<String> & cantBeBeforeK, const Queue<String> & cantBeAfterK)
{
   for (uint32 i=0; i<seqB.GetNumItems(); i++)
   {
      if (ExtractPointerString(seqB[i]) == key)
      {
         for (uint32 j=0;   j<i;                  j++) if (cantBeBeforeK.Contains(ExtractPointerString(seqB[j]))) return B_ERROR;
         for (uint32 j=i+1; j<seqB.GetNumItems(); j++) if (cantBeAfterK.Contains( ExtractPointerString(seqB[j]))) return B_ERROR;
         break;
      }
   }
   return B_NO_ERROR;
}

static void PrintSequence(uint32 i, const Queue<String> & seq, const char * desc, const Hashtable<unsigned long, Void> & threads)
{
   printf("\n%s SEQUENCE " UINT32_FORMAT_SPEC " (" UINT32_FORMAT_SPEC " threads ", desc, i, threads.GetNumItems());
   bool isFirst = true;
   for (HashtableIterator<unsigned long, Void> iter(threads); iter.HasData(); iter++) 
   {
      if (isFirst == false) printf(", ");
      isFirst = false;
      printf("%lu", iter.GetKey());
   }
   printf("):\n");
   for (uint32 j=0; j<seq.GetNumItems(); j++) printf("   " UINT32_FORMAT_SPEC ".  %s\n", j, seq[j]());
}

// This program reads debug output to look for potential deadlocks
int main(void) 
{
   Queue<Queue<String> > maxLogs;

   Hashtable<uint32, Hashtable<unsigned long, Void> > sequenceToThreads;
   Hashtable<unsigned long, Queue<String> > curLockState;
   char buf[1024];
   while(fgets(buf, sizeof(buf), stdin))
   {
      String s = buf; s = s.Trim();
      StringTokenizer tok(s());
      const String actionStr = tok();
      if ((actionStr == "mx_lock:")||(actionStr == "mx_unlk:"))
      {
         String threadStr = tok();
         String mutexStr  = tok();
         String locStr    = tok();
         if ((threadStr.StartsWith("tid="))&&(mutexStr.StartsWith("m="))&&(locStr.StartsWith("loc=")))
         {
            threadStr = threadStr.Substring(4);
            mutexStr  = mutexStr.Substring(2);
            locStr    = locStr.Substring(4);

            //printf("thread=[%s] actionStr=[%s] mutexStr=[%s] locStr=[%s]\n", threadStr(), actionStr(), mutexStr(), locStr());
            const String lockDesc = mutexStr + " & " + locStr;

            const unsigned long threadID = Atoull(threadStr());
            if (actionStr == "mx_lock:")
            {
               Queue<String> * q = curLockState.GetOrPut(threadID, Queue<String>());
               q->AddTail(lockDesc);
               if (q->GetNumItems() > 1)
               {
                  // See if we have this queue logged anywhere already
                  int32 logIdx = -1;
                  for (uint32 i=0; i<maxLogs.GetNumItems(); i++)
                  {
                     if (LogsMatchExceptForLocation(maxLogs[i], *q))
                     {
                        logIdx = i;
                        break;
                     }
                  }
                  if (logIdx < 0) {maxLogs.AddTail(*q); logIdx = maxLogs.GetNumItems()-1;}

                  // Keep track of which thread(s) used this sequence
                  Hashtable<unsigned long, Void> * seqToThread = sequenceToThreads.GetOrPut(logIdx);
                  seqToThread->PutWithDefault(threadID);
               }
            }
            else
            {
               Queue<String> * q = curLockState.Get(threadID);
               if (q)
               {
                  // Find the last instance of this mutex in our list
                  String lockName = lockDesc; {int32 lastAmp = lockName.LastIndexOf('&'); lockName = lockName.Substring(0, lastAmp);}
                  bool foundLock = false;
                  for (int32 i=q->GetNumItems()-1; i>=0; i--)
                  {
                     String nextStr = (*q)[i]; {int32 lastAmp = nextStr.LastIndexOf('&'); nextStr = nextStr.Substring(0, lastAmp);}
                     if (nextStr == lockName)
                     {
                        foundLock = true;
                        q->RemoveItemAt(i);
                        break;
                     }
                  }
                  if (foundLock == false) printf("ERROR:  thread %lu is unlocking a lock he never locked!!! [%s]\n", threadID, lockDesc());
                  if (q->IsEmpty()) curLockState.Remove(threadID);
               }
               else printf("ERROR:  thread %lu is unlocking when he has nothing locked!!! [%s]\n", threadID, lockDesc());
            }
         }
         else printf("ERROR PARSING OUTPUT LINE [%s]\n", s());
#ifdef NOT_CURRENTLY_USED
         PrintState(curLockState);
#endif
      }
   }

   for (HashtableIterator<unsigned long, Queue<String> > iter(curLockState); iter.HasData(); iter++)
   {
      printf("\n\nERROR, AT END OF PROCESSING, LOCKS WERE STILL HELD BY THREAD %lu:\n", iter.GetKey());
      const Queue<String> & q = iter.GetValue();
      for (uint32 i=0; i<q.GetNumItems(); i++) printf("  " UINT32_FORMAT_SPEC ". %s\n", i, q[i]());
   }

   printf("\n------------------- BEGIN UNIQUE LOCK SEQUENCES -----------------\n");
   for (uint32 i=0; i<maxLogs.GetNumItems(); i++) if (maxLogs[i].GetNumItems() > 1) PrintSequence(i, maxLogs[i], "UNIQUE", *sequenceToThreads.Get(i));
   printf("\n------------------- END UNIQUE LOCK SEQUENCES -----------------\n\n");

   // Now see if there are any inconsistencies.  First, go through the sequences and remove any redundant re-locks, as they don't count
   Queue<Queue<String> > simplifiedMaxLogs = maxLogs;
   for (uint32 i=0; i<simplifiedMaxLogs.GetNumItems(); i++)
   { 
      Hashtable<String, uint32> useCounts;
      Queue<String> & seqA = simplifiedMaxLogs[i];
      for (uint32 j=0; j<seqA.GetNumItems(); j++) (*useCounts.GetOrPut(ExtractPointerString(seqA[j])))++;
      for (HashtableIterator<String, uint32> iter(useCounts); iter.HasData(); iter++)
      {
         const uint32 numToRemove = iter.GetValue()-1;
         for (int32 j=seqA.GetNumItems()-1; ((numToRemove>0)&&(j>=0)); j--) if (ExtractPointerString(seqA[j]) == iter.GetKey()) seqA.RemoveItemAt(j);
      }
      useCounts.Clear();
   }

   // Then do the inconsistencies check
   bool foundProblems = false;
   for (uint32 i=0; i<simplifiedMaxLogs.GetNumItems(); i++)
   {
      const Queue<String> & seqA = simplifiedMaxLogs[i];
      if (seqA.GetNumItems() > 1)
      {
         for (uint32 j=0; j<simplifiedMaxLogs.GetNumItems(); j++)
         {
            const Queue<String> & seqB = simplifiedMaxLogs[j];
            if ((i < j)&&(seqB.GetNumItems() > 1))  // the (i<j) is to rule out testing something against itself, and to eliminate symmetric duplicate reports (e.g. 15 vs 20 AND 20 vs 15)
            {
               for (uint32 k=0; k<seqA.GetNumItems(); k++)
               {
                  Queue<String> cantBeAfterK;  for (uint32 m=0;   m<k;                  m++) cantBeAfterK.AddTail( ExtractPointerString(seqA[m]));
                  Queue<String> cantBeBeforeK; for (uint32 m=k+1; m<seqA.GetNumItems(); m++) cantBeBeforeK.AddTail(ExtractPointerString(seqA[m]));
                  if (CheckOrderingConstraints(ExtractPointerString(seqA[k]), seqB, cantBeBeforeK, cantBeAfterK).IsError())
                  {
                     foundProblems = true;

                     // If both patterns were only made within a single thread, then the ordering isn't currently harmful, only potentially so
                     Hashtable<unsigned long, Void> * ti = sequenceToThreads.Get(i);
                     Hashtable<unsigned long, Void> * tj = sequenceToThreads.Get(j);
                     const bool isDefinite = ((ti)&&(tj)&&((ti->GetNumItems() > 1)||(tj->GetNumItems() > 1)||(*ti != *tj)));   // (ti) and (tj) are actually never going to be NULL, but this makes clang++ happy
     
                     printf("\n\n------------------------------------------\n");
                     printf("ERROR, %s LOCK-ACQUISITION ORDERING INCONSISTENCY DETECTED:   SEQUENCE #" UINT32_FORMAT_SPEC " vs SEQUENCE #" UINT32_FORMAT_SPEC "  !!\n", isDefinite?"DEFINITE":"POTENTIAL", i, j);
                     PrintSequence(i, maxLogs[i], "PROBLEM", *sequenceToThreads.Get(i));
                     PrintSequence(j, maxLogs[j], "PROBLEM", *sequenceToThreads.Get(j));
                     break;
                  }
               }
            }
         }
      }
   }
   if (foundProblems == false) printf("No Mutex-acquisition ordering problems detected, yay!\n");

   return 0;
}
