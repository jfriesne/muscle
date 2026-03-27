/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

static status_t Kick(MessageIOGateway & gw, const char * arg);
status_t Kick(MessageIOGateway & gw, const char * arg)
{
   MessageRef msg = GetMessageFromPool(PR_COMMAND_KICK);
   MRETURN_ON_ERROR(msg);

   String str("/");
   str += arg;
   str += "/*";
   MRETURN_ON_ERROR(msg()->AddString(PR_NAME_KEYS, str));
   MRETURN_ON_ERROR(gw.AddOutgoingMessage(msg));
   LogTime(MUSCLE_LOG_INFO, "Kicking users matching pattern [%s]\n", str.Cstr());

   return B_NO_ERROR;
}

static status_t Ban(MessageIOGateway & gw, const char * arg, bool unBan);
status_t Ban(MessageIOGateway & gw, const char * arg, bool unBan)
{
   MessageRef msg = GetMessageFromPool(unBan ? PR_COMMAND_REMOVEBANS : PR_COMMAND_ADDBANS);
   MRETURN_ON_ERROR(msg);
   MRETURN_ON_ERROR(msg()->AddString(PR_NAME_KEYS, arg));
   MRETURN_ON_ERROR(gw.AddOutgoingMessage(msg));
   if (unBan) LogTime(MUSCLE_LOG_INFO, "Removing ban patterns that match pattern [%s]\n", arg);
         else LogTime(MUSCLE_LOG_INFO, "Adding ban pattern [%s]\n", arg);

   return B_NO_ERROR;
}

static status_t Require(MessageIOGateway & gw, const char * arg, bool unRequire);
status_t Require(MessageIOGateway & gw, const char * arg, bool unRequire)
{
   MessageRef msg = GetMessageFromPool(unRequire ? PR_COMMAND_REMOVEREQUIRES : PR_COMMAND_ADDREQUIRES);
   MRETURN_ON_ERROR(msg);

   MRETURN_ON_ERROR(msg()->AddString(PR_NAME_KEYS, arg));
   MRETURN_ON_ERROR(gw.AddOutgoingMessage(msg));
   if (unRequire) LogTime(MUSCLE_LOG_INFO, "Removing require patterns that match pattern [%s]\n", arg);
         else LogTime(MUSCLE_LOG_INFO, "Adding require pattern [%s]\n", arg);

   return B_NO_ERROR;
}

// This is a little admin program, useful for kicking, banning, or unbanning users
// without having to restart the MUSCLE server.  Example command line:
//   admin server=muscleserver.mycompany.com kick=192.168.0.23 ban=16.25.29.2 kickban=1.2.3.4 unban=1.2.3.4 ban=2.3.4.5 ban=3.4.5.*
// Note that you can only do this if your IP address has the requisite privileges on
// the MUSCLE server!  (i.e. to ban, the server must have been run with an argument like privban=your.ip.address)
#ifdef UNIFIED_DAEMON
int admin_main(int argc, char ** argv)
#else
int main(int argc, char ** argv)
#endif
{
   CompleteSetupSystem css;

   const char * hostName = "localhost";

   // First, find out if there is a server specified.
   for (int i=1; i<argc; i++)
   {
      char * next = argv[i];
           if (strncmp(next, "server=", 7) == 0) hostName = &next[7];
      else if (strcmp(next, "help") == 0)
      {
         LogTime(MUSCLE_LOG_INFO, "This program lets you send admin commands to a running MUSCLE server.\n");
         LogTime(MUSCLE_LOG_INFO, "Note that the MUSCLE server will not listen to your commands unless your ip address was\n");
         LogTime(MUSCLE_LOG_INFO, "specified as a privileged IP address in its command line arguments [e.g. ./muscled privall=your.IP.address]\n");
         LogTime(MUSCLE_LOG_INFO, "Usage:  admin [server=localhost] [ban=pattern] [unban=pattern] [kick=pattern] [kickban=pattern]\n");
         return 0;
      }
   }

   const char * cln = strchr(hostName, ':');
   const uint16 port = cln ? (uint16) atoi(cln+1) : 2960;

   ConstSocketRef s = Connect(String(hostName).Substring(0, ":")(), port, "admin", false);
   if (s() == NULL)
   {
      LogTime(MUSCLE_LOG_INFO, "(run 'admin help' for arguments)\n");
      return 10;
   }

   MessageIOGateway gw;
   gw.SetDataIO(DataIORef(new TCPSocketDataIO(s, false)));

   // Now generate all commands...
   for (int j=1; j<argc; j++)
   {
      char * line = argv[j];
      char * colon = strchr(line, '=');
      if (colon)
      {
         char * arg = colon+1;
         *colon = '\0';

         status_t r;

              if (strcmp(line, "kick")      == 0) r = Kick(gw, arg);
         else if (strcmp(line, "ban")       == 0) r = Ban(gw, arg, false);
         else if (strcmp(line, "unban")     == 0) r = Ban(gw, arg, true);
         else if (strcmp(line, "require")   == 0) r = Require(gw, arg, false);
         else if (strcmp(line, "unrequire") == 0) r = Require(gw, arg, true);
         else if (strcmp(line, "kickban")   == 0)
         {
            r  = Kick(gw, arg);
            r |= Ban(gw, arg, false);
         }

         if (r.IsError())
         {
            LogTime(MUSCLE_LOG_ERROR, "[%s] command generation failed [%s]\n", line, r());
            return 10;
         }
      }
   }

   // Lastly, request a PONG so we know when all has been done
   if (gw.AddOutgoingMessage(MessageRef(GetMessageFromPool(PR_COMMAND_PING))).IsError())
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't queue ping!\n");
      return 10;
   }

   // send them, and then wait for the pong back
   const uint64 timeoutTime = GetRunTime64() + SecondsToMicros(30);
   uint32 errorCount = 0;
   QueueGatewayMessageReceiver inQueue;
   SocketMultiplexer multiplexer;
   while(s())
   {
      const int fd = s()->GetFileDescriptor();
      (void) multiplexer.RegisterSocketForReadReady(fd);
      if (gw.HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(fd);

      status_t ret;
      if (multiplexer.WaitForEvents(timeoutTime).IsError(ret))
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "WaitForEvents() failed, exiting! [%s]\n", ret());
         errorCount++;
         break;
      }

      const bool readError  = ((multiplexer.IsSocketReadyForRead(fd)) &&(gw.DoInput(inQueue).IsError()));
      const bool writeError = ((multiplexer.IsSocketReadyForWrite(fd))&&(gw.DoOutput().IsError()));
      if ((readError)||(writeError))
      {
         LogTime(MUSCLE_LOG_ERROR, "TCP connection was cut prematurely!\n");
         errorCount++;
         break;
      }

      MessageRef incoming;
      while(inQueue.RemoveHead(incoming).IsOK())
      {
         const Message * msg = incoming();
         if (msg)
         {
            switch(msg->what)
            {
               case PR_RESULT_PONG:
                  s.Reset();
               break;

               case PR_RESULT_ERRORACCESSDENIED:
               {
                  errorCount++;
                  LogTime(MUSCLE_LOG_ERROR, "Access denied!  ");
                  const char * who;
                  ConstMessageRef subMsg;
                  if ((msg->FindMessage(PR_NAME_REJECTED_MESSAGE, subMsg).IsOK())&&(subMsg()->FindString(PR_NAME_KEYS, &who).IsOK()))
                  {
                     const char * action = "do that to";
                     switch(subMsg()->what)
                     {
                        case PR_COMMAND_KICK:           action = "kick";      break;
                        case PR_COMMAND_ADDBANS:        action = "ban";       break;
                        case PR_COMMAND_REMOVEBANS:     action = "unban";     break;
                        case PR_COMMAND_ADDREQUIRES:    action = "require";   break;
                        case PR_COMMAND_REMOVEREQUIRES: action = "unrequire"; break;
                     }
                     LogPlain(MUSCLE_LOG_ERROR, "You are not allowed to %s [%s]!", action, who);
                  }
                  LogPlain(MUSCLE_LOG_ERROR, "\n");
               }
               break;
            }
         }
      }
   }
   LogTime(MUSCLE_LOG_INFO, "Exiting. (" UINT32_FORMAT_SPEC " errors)\n", errorCount);

   return (errorCount>0) ? 10 : 0;
}
