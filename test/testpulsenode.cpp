#include <stdio.h>
#include <stdlib.h>

#include "dataio/NullDataIO.h"
#include "reflector/AbstractReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/PulseNode.h"
#include "util/String.h"

using namespace muscle;

// This test creates and exercises a large number of PulseNodes, just to be sure that
// such a thing can be done without too much inefficiency.
static const int NUM_PULSE_CHILDREN = 100000; // an unreasonable number to be sure, but we want to be scalable... :^)
static const uint64 PULSE_INTERVAL  = 50000;  // have one child fire every 50

class TestPulseChild : public PulseNode
{
public:
   TestPulseChild(uint64 baseTime, int idx)
      : _fireTime(baseTime+(idx*PULSE_INTERVAL))
      , _idx(idx)
   {
      LogTime(MUSCLE_LOG_INFO, "TestPulseChild %i (%p) Initially scheduled for " UINT64_FORMAT_SPEC " (time until = " INT64_FORMAT_SPEC ")\n", idx, this, _fireTime, _fireTime-GetRunTime64());
   }

   virtual uint64 GetPulseTime(const PulseArgs &) {return _fireTime;}

   virtual void Pulse(const PulseArgs & args)
   {
      _fireTime = args.GetScheduledTime() + (NUM_PULSE_CHILDREN*PULSE_INTERVAL);
      LogTime(MUSCLE_LOG_INFO, "TestPulseChild %i (%p) Pulsed at " UINT64_FORMAT_SPEC "/" UINT64_FORMAT_SPEC " (diff=" INT64_FORMAT_SPEC "), next pulse time will be " UINT64_FORMAT_SPEC "\n", _idx, this, args.GetCallbackTime(), args.GetScheduledTime(), args.GetCallbackTime()-args.GetScheduledTime(), _fireTime);

      CheckForEndOfTest();
   }

private:
   void CheckForEndOfTest();

   uint64 _fireTime;
   int _idx;
};

class TestSession : public AbstractReflectSession
{
public:
   TestSession(int maxCount) : _maxCount(maxCount)
   {
      // empty
   }

   virtual status_t AttachedToServer()
   {
      LogTime(MUSCLE_LOG_INFO, "TestSession::AttachedToServer() called...\n");

      MRETURN_ON_ERROR(AbstractReflectSession::AttachedToServer());

      const uint64 baseTime = GetRunTime64();
      for (int i=0; i<NUM_PULSE_CHILDREN; i++)
      {
         TestPulseChild * tpc = new TestPulseChild(baseTime, i);
         status_t ret;
         if ((_tpcs.AddTail(tpc).IsError(ret))||(PutPulseChild(tpc).IsError(ret))) LogTime(MUSCLE_LOG_CRITICALERROR, "Error [%s] creating pulse child #%i!\n", ret(), i);
      }
      return B_NO_ERROR;
   }

   virtual void AboutToDetachFromServer()
   {
      LogTime(MUSCLE_LOG_INFO, "TestSession::AboutToDetachFromServer() called...\n");
      AbstractReflectSession::AboutToDetachFromServer();

      for (int32 i=_tpcs.GetNumItems()-1; i>=0; i--) delete _tpcs[i];
      _tpcs.Clear();
   }

   virtual void MessageReceivedFromGateway(const MessageRef &, void *) {/* empty */}

   void CheckForEndOfTest() {if ((_maxCount > 0)&&(--_maxCount == 0)) EndServer();}

private:
   int _maxCount;
   Queue<TestPulseChild *> _tpcs;
};

void TestPulseChild :: CheckForEndOfTest()
{
   static_cast<TestSession *>(GetPulseParent())->CheckForEndOfTest();
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;  // set up our environment

   Message args; (void) ParseArgs(argc, argv, args);
   HandleStandardDaemonArgs(args);

   ReflectServer server;
   TestSession session(args.HasName("fromscript") ? 100 : -1);

   status_t ret;
   if (server.AddNewSession(DummyAbstractReflectSessionRef(session)).IsOK(ret))
   {
      LogTime(MUSCLE_LOG_INFO, "Beginning PulseNode test...\n");
      if (server.ServerProcessLoop().IsOK(ret)) LogTime(MUSCLE_LOG_INFO, "testpulsechild event loop exiting.\n");
                                           else LogTime(MUSCLE_LOG_CRITICALERROR, "testpulsechild event loop exiting with error condition [%s].\n", ret());
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "AddNewSession() failed [%s]\n", ret());

   server.Cleanup();

   return ret.IsOK() ? 0 : 10;
}
