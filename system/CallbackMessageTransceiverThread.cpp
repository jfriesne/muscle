/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/CallbackMessageTransceiverThread.h"

namespace muscle {

void CallbackMessageTransceiverThread :: DispatchCallbacks(uint32)
{
   // deliberately not calling up to the superclass!

   uint32 code;
   MessageRef next;
   String sessionID;
   uint32 factoryID;
   IPAddressAndPort iap;
   bool seenIncomingMessage = false;

   // Check for any new messages from our internal thread
   while(GetNextEventFromInternalThread(code, &next, &sessionID, &factoryID, &iap) >= 0)
   {
      switch(code)
      {
         case MTT_EVENT_INCOMING_MESSAGE: default:
            if (seenIncomingMessage == false)
            {
               seenIncomingMessage = true;
               BeginMessageBatch();
            }
            MessageReceived(next, sessionID);
         break;

         case MTT_EVENT_SESSION_ACCEPTED:      SessionAccepted(sessionID, factoryID, iap); break;
         case MTT_EVENT_SESSION_ATTACHED:      SessionAttached(sessionID);                 break;
         case MTT_EVENT_SESSION_CONNECTED:     SessionConnected(sessionID, iap);           break;
         case MTT_EVENT_SESSION_DISCONNECTED:  SessionDisconnected(sessionID);             break;
         case MTT_EVENT_SESSION_DETACHED:      SessionDetached(sessionID);                 break;
         case MTT_EVENT_FACTORY_ATTACHED:      FactoryAttached(factoryID);                 break;
         case MTT_EVENT_FACTORY_DETACHED:      FactoryDetached(factoryID);                 break;
         case MTT_EVENT_OUTPUT_QUEUES_DRAINED: OutputQueuesDrained(next);                  break;
         case MTT_EVENT_SERVER_EXITED:         ServerExited();                             break;
      }
      InternalThreadEvent(code, next, sessionID, factoryID);  // this get called for any event
   }

   if (seenIncomingMessage) EndMessageBatch();
}

void CallbackMessageTransceiverThread :: BeginMessageBatch()
{
   // empty
}

void CallbackMessageTransceiverThread :: MessageReceived(const MessageRef &, const String &)
{
   // empty
}

void CallbackMessageTransceiverThread :: EndMessageBatch()
{
   // empty
}

void CallbackMessageTransceiverThread :: SessionAccepted(const String &, uint32, const IPAddressAndPort &)
{
   // empty
}

void CallbackMessageTransceiverThread :: SessionAttached(const String &)
{
   // empty
}

void CallbackMessageTransceiverThread :: SessionConnected(const String &, const IPAddressAndPort &)
{
   // empty
}

void CallbackMessageTransceiverThread :: SessionDisconnected(const String &)
{
   // empty
}

void CallbackMessageTransceiverThread :: SessionDetached(const String &)
{
   // empty
}

void CallbackMessageTransceiverThread :: FactoryAttached(uint32)
{
   // empty
}

void CallbackMessageTransceiverThread :: FactoryDetached(uint32)
{
   // empty
}

void CallbackMessageTransceiverThread :: ServerExited()
{
   // empty
}

void CallbackMessageTransceiverThread :: OutputQueuesDrained(const MessageRef &)
{
   // empty
}

void CallbackMessageTransceiverThread :: InternalThreadEvent(uint32, const MessageRef &, const String &, uint32)
{
   // empty
}

} // end namespace muscle
