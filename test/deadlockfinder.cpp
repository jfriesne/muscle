/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "system/SetupSystem.h"
#include "util/Hashtable.h"
#include "util/Queue.h"
#include "util/String.h"

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

// Returns true iff (seqA)'s locking-order is inconsistent with (seqB)'s locking-order
static bool SequencesAreInconsistent(const Queue<String> & seqA, const Queue<String> & seqB)
{
   const Queue<String> & largerQ  = (seqA.GetNumItems()  > seqB.GetNumItems()) ? seqA : seqB;
   const Queue<String> & smallerQ = (seqA.GetNumItems() <= seqB.GetNumItems()) ? seqA : seqB;
//printf("CHECK %u/%u seqA=%p seqB=%p larger=%p smaller=%p\n",seqA.GetNumItems(), seqB.GetNumItems(), &seqA, &seqB, &largerQ, &smallerQ);
//printf("largerQ="); for (uint32 i=0; i<largerQ.GetNumItems(); i++) printf("%s,", largerQ[i]()); printf("\n");
//printf("smallerQ="); for (uint32 i=0; i<smallerQ.GetNumItems(); i++) printf("%s,", smallerQ[i]()); printf("\n");

   for (uint32 i=0; i<largerQ.GetNumItems(); i++)
      for (uint32 j=0; ((j<i)&&(j<largerQ.GetNumItems())); j++)
      {
         const String & largerIStr = largerQ[i];
         const String & largerJStr = largerQ[j];
         const int32 smallerIPos = smallerQ.IndexOf(largerIStr);
         const int32 smallerJPos = smallerQ.IndexOf(largerJStr);
//printf("   i=%u j=%u largerIStr=[%s] largerJStr=[%s] smallerIPos=%i smallerJPos=%i\n", i, j, largerIStr(), largerJStr(), smallerIPos, smallerJPos);
         if ((smallerIPos >= 0)&&(smallerJPos >= 0)&&((i<j) != (smallerIPos < smallerJPos))) return true;
      }

   return false;
}

static String LockSequenceToString(const Queue<String> & seq)
{
   String ret;
   for (uint32 i=0; i<seq.GetNumItems(); i++)
   {
      if (ret.HasChars()) ret += ',';
      ret += seq[i];
   }
   return ret;
}

static void PrintSequenceReport(const char * desc, const Queue<String> & seq, const Hashtable<String, Queue<String> > & details)
{
   printf("  %s: [%s] was executed by " UINT32_FORMAT_SPEC " threads:\n", desc, LockSequenceToString(seq)(), details.GetNumItems());

   Hashtable<Queue<String>, String> detailsToThreads;
   {
      for (HashtableIterator<String, Queue<String> > iter(details); iter.HasData(); iter++)
      {
         String * threadsList = detailsToThreads.GetOrPut(iter.GetValue());
         if (threadsList->HasChars()) (*threadsList) += ',';
         (*threadsList) += iter.GetKey();
      }
   }

   for (HashtableIterator<Queue<String>, String> iter(detailsToThreads); iter.HasData(); iter++)
   {
      printf("    Thread%s [%s] locked mutexes in this order:\n", iter.GetValue().Contains(',')?"s":"", iter.GetValue()());
      const Queue<String> & details = iter.GetKey();
      for (uint32 i=0; i<details.GetNumItems(); i++) printf("       " UINT32_FORMAT_SPEC ": %s\n", i, details[i]());
   }
}

// This program reads debug output to look for potential deadlocks
int main(int, char **)
{
   CompleteSetupSystem css;

   Hashtable< Queue<String>, Hashtable<String, Queue<String> > > mutexLockSequenceToThreads;  // keys are the sequence of mutex-pointers locked, values are the set of threads that locked in that sequence, each with its own details
 
   // First, read in the input
   {
      String curThreadID;
      Queue<String> curMutexes;
      Queue<String> curDetails;

      char buf[1024];
      while(fgets(buf, sizeof(buf), stdin))
      {
         if (strncmp(buf, "dlf: ", 5) == 0)
         {
            String s = &buf[5]; s = s.Trim();  // either BEGIN_THREAD, END_THREAD, BEGIN_LOCK_SEQUENCE, or END_LOCK_SEQUENCE

            if ((s.StartsWith("BEGIN_"))||(s.StartsWith("END_")))
            {
               if ((curThreadID.HasChars())&&(curDetails.HasItems())) (void) mutexLockSequenceToThreads.GetOrPut(curMutexes)->GetOrPut(curThreadID, curDetails);
               curMutexes.Clear();
               curDetails.Clear();

                    if (s.StartsWith("BEGIN_THREAD")) curThreadID = s.Substring(13).Trim();
               else if (s.StartsWith("END_THREAD"))   curThreadID.Clear();
            }
            else if (s.StartsWith("m="))
            {
               const String mutexID = s.Substring(2).Substring(0, " ");
               (void) curMutexes.AddTailIfNotAlreadyPresent(mutexID); // recursively re-locking a mutex that the thread already had locked doesn't make a difference as far as deadlocks are concerned
               (void) curDetails.AddTail(s);  // but we'll include it in the details anyway, in case it helps the user figure out what is going on
            }
         }
      }
   }

   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "------------------- " UINT32_FORMAT_SPEC " UNIQUE LOCK SEQUENCES DETECTED -----------------\n", mutexLockSequenceToThreads.GetNumItems());
   for (HashtableIterator<Queue<String>, Hashtable<String, Queue<String> > > iter(mutexLockSequenceToThreads); iter.HasData(); iter++)
      LogTime(MUSCLE_LOG_INFO, "LockSequence [%s] was executed by " UINT32_FORMAT_SPEC " threads\n", LockSequenceToString(iter.GetKey())(), iter.GetValue().GetNumItems());

   // Now we check for inconsistent locking order.  Two sequences are inconsistent with each other if they lock the same two mutexes
   // but lock them in a different order
   Hashtable<uint32, uint32> inconsistentSequencePairs;  // smaller key-position -> larger key-position
   {
      uint32 idxA = 0;
      for (HashtableIterator<Queue<String>, Hashtable<String, Queue<String> > > iterA(mutexLockSequenceToThreads); iterA.HasData(); iterA++,idxA++)
      {
         uint32 idxB = 0;
         for (HashtableIterator<Queue<String>, Hashtable<String, Queue<String> > > iterB(mutexLockSequenceToThreads); ((idxB < idxA)&&(iterB.HasData())); iterB++,idxB++)
            if (SequencesAreInconsistent(iterB.GetKey(), iterA.GetKey())) (void) inconsistentSequencePairs.Put(idxB, idxA);
      }
   }

   bool foundProblems = false;
   if (inconsistentSequencePairs.HasItems())
   {
      printf("\n");
      LogTime(MUSCLE_LOG_WARNING, "--------- WARNING: " UINT32_FORMAT_SPEC " INCONSISTENT LOCK SEQUENCE%s DETECTED --------------\n", inconsistentSequencePairs.GetNumItems(), (inconsistentSequencePairs.GetNumItems()==1)?"":"S");
      uint32 idx = 0;
      for (HashtableIterator<uint32, uint32> iter(inconsistentSequencePairs); iter.HasData(); iter++,idx++)
      {
         printf("\n");
         LogTime(MUSCLE_LOG_WARNING, "INCONSISTENT LOCKING ORDER REPORT #" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " --------\n", idx+1, inconsistentSequencePairs.GetNumItems());
         const Queue<String> & seqA = *mutexLockSequenceToThreads.GetKeyAt(iter.GetKey());
         const Queue<String> & seqB = *mutexLockSequenceToThreads.GetKeyAt(iter.GetValue());
         PrintSequenceReport("SequenceA", seqA, mutexLockSequenceToThreads[seqA]);
         PrintSequenceReport("SequenceB", seqB, mutexLockSequenceToThreads[seqB]);
         foundProblems = true;
      }
   }

   if (foundProblems == false)
   {
      printf("\n");
      LogTime(MUSCLE_LOG_INFO, "No Mutex-acquisition ordering problems detected, yay!\n");
   }

   printf("\n\n");
   return 0;
}
