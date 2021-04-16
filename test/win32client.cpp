#include <stdio.h>

#include "winsupport/Win32MessageTransceiverThread.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "system/SetupSystem.h"

using namespace muscle;

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Win32AllocateStdioConsole();

   char * hostName = "localhost";
   uint16 port = 0;

   if (argc > 1) hostName = argv[1];
   if (argc > 2) port     = (uint16) atoi(argv[2]);
   if (port <= 0) port    = 2960;

   Win32MessageTransceiverThread mtt(CreateEvent(0, false, false, 0), true);

   printf("Connecting to host=[%s] port=%i\n", hostName, port);
   if ((mtt.StartInternalThread().IsOK())&&(mtt.AddNewConnectSession(hostName, port).IsOK()))
   {
      // The only thing this example needs to wait for notification on
      // is the MessageTransceiverThread's signal-handle.  A real-life
      // application would probably need to wait on other things too,
      // in which case those handles would go into this array also.
      ::HANDLE waitObjects[] = {mtt.GetSignalHandle()};

      bool keepGoing = true;
      while(keepGoing)
      {
         // Wait for next event or timeout
         const int waitResult = WaitForMultipleObjects(ARRAYITEMS(waitObjects), waitObjects, false, 1000);
         if (waitResult == WAIT_TIMEOUT)
         {
            MessageRef msg = GetMessageFromPool(PR_COMMAND_GETPARAMETERS);
            if (msg()) 
            {
               printf("Sending PR_COMMAND_GETPARAMETERS message to server...\n");
               mtt.SendMessageToSessions(msg);
            }
         }
         else if (waitResult == WAIT_OBJECT_0)
         {
            // Hey, the Win32MessageTransceiverThread says he has something for us!
            uint32 eventCode;
            MessageRef msg;
            while(mtt.GetNextEventFromInternalThread(eventCode, &msg) >= 0)
            {
               switch(eventCode)
               {
                  case MTT_EVENT_INCOMING_MESSAGE: 
                     printf("EVENT: A new message from the remote computer is ready to process.  The Message is:\n");
                     if (msg()) msg()->PrintToStream();
                  break;

                  case MTT_EVENT_SESSION_ACCEPTED:              
                     printf("EVENT: A new session has been created by one of our factory objects\n");
                  break;

                  case MTT_EVENT_SESSION_ATTACHED:              
                     printf("EVENT: A new session has been attached to the local server\n");
                  break;

                  case MTT_EVENT_SESSION_CONNECTED:             
                     printf("EVENT: A session on the local server has completed its connection to the remote one\n");
                  break;

                  case MTT_EVENT_SESSION_DISCONNECTED:          
                     printf("EVENT: A session on the local server got disconnected from its remote peer\n");
                     keepGoing = false;  // no sense in continuing now!
                  break;

                  case MTT_EVENT_SESSION_DETACHED:              
                     printf("EVENT: A session on the local server has detached (and been destroyed)\n");
                  break;

                  case MTT_EVENT_FACTORY_ATTACHED:              
                     printf("EVENT: A ReflectSessionFactory object has been attached to the server\n");
                  break;

                  case MTT_EVENT_FACTORY_DETACHED:              
                     printf("EVENT: A ReflectSessionFactory object has been detached (and been destroyed)\n");
                  break;

                  case MTT_EVENT_OUTPUT_QUEUES_DRAINED:         
                     printf("EVENT: Output queues of sessions previously specified in RequestOutputQueuesDrainedNotification() have drained\n");
                  break;

                  case MTT_EVENT_SERVER_EXITED:                 
                     printf("EVENT: The ReflectServer event loop has terminated\n");
                  break;

                  default:
                     printf("EVENT: Unknown event code " UINT32_FORMAT_SPEC " from Win32MessageTransceiverThread!?\n", eventCode);
                  break;
               }
            }
         }
      }
      printf("Shutting down MessageTransceiverThread...\n");
   }
   else printf("Error, could not start Win32MessageTransceiverThread!\n");

   mtt.Reset();  // important, to avoid race conditions in the destructor!!

   return 0;
}

