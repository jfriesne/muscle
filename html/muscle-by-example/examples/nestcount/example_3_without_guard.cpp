#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/NestCount.h"
#include "util/Hashtable.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates the use of a NestCount without a NestCountGuard\n");
   printf("(Sometimes you can't use a NestCountGuard because your processing-batches\n");
   printf("are spread out across function calls that you don't directly control,\n");
   printf("and thus you can't easily put a NestCountGuard high enough up in the call tree)\n");
   printf("\n");
}

class MyClass
{
public:
   MyClass() {/* empty */}

   void BeginBatch()
   {
      if (_inBatch.Increment()) printf("Entering batch mode...\n");
   }

   void QueueValue(int qv)
   {
      if (_inBatch.IsInBatch()) _queuedValues.PutWithDefault(qv);  // we'll call ProcessValue() at the end of the batch
                           else ProcessValue(qv);  // if we're not in a batch, then process the value right now
   }

   void EndBatch()
   {
      if (_inBatch.Decrement())
      {
         printf("End of batch.  Processing all the values that were queued up during the batch:\n");
         for (HashtableIterator<int, Void> iter(_queuedValues); iter.HasData(); iter++) ProcessValue(iter.GetKey());
         _queuedValues.Clear();
      }
   }

private:
   void ProcessValue(int v)
   {
      printf("   PROCESSING VALUE %i\n", v);  // well, pretending to, anyway -- imagine some expensive operation here
   }

   NestCount _inBatch;
   Hashtable<int, Void> _queuedValues;
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   MyClass c;

   c.QueueValue(5);
   c.QueueValue(6);
   c.QueueValue(7);
   c.QueueValue(7);  // oh dear, no batch-mode means we gotta process 7 twice

   c.BeginBatch();
      c.QueueValue(8);
      c.QueueValue(9);
      c.QueueValue(10);
      c.QueueValue(8);
      c.QueueValue(10);  // by using a batch-mode we avoid processing duplicate values (like this one) more than once
      c.QueueValue(11);
   c.EndBatch();         // all non-duplicate values in the batch will get processed here

   c.QueueValue(11);
   c.QueueValue(12);

   c.BeginBatch();
      c.QueueValue(13);
      c.QueueValue(14);
      c.QueueValue(14);
      c.QueueValue(15);
      c.BeginBatch();
         c.QueueValue(15);
         c.QueueValue(16);
         c.QueueValue(17);
         c.QueueValue(13);
         c.QueueValue(12);
      c.EndBatch();
   c.EndBatch();         // all non-duplicate values in the batch will get processed here

   printf("\n");
   return 0;
}
