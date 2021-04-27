/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

static void Kick(MessageIOGateway & gw, const char * arg);
void Kick(MessageIOGateway & gw, const char * arg)
{
   MessageRef msg = GetMessageFromPool(PR_COMMAND_KICK);
   if (msg())
   {
      String str("/");
      str += arg;
      str += "/*";
      (void) msg()->AddString(PR_NAME_KEYS, str);
      (void) gw.AddOutgoingMessage(msg);
      LogTime(MUSCLE_LOG_INFO, "Kicking users matching pattern [%s]\n", str.Cstr());
   }
}

static void Ban(MessageIOGateway & gw, const char * arg, bool unBan);
void Ban(MessageIOGateway & gw, const char * arg, bool unBan)
{
   MessageRef msg = GetMessageFromPool(unBan ? PR_COMMAND_REMOVEBANS : PR_COMMAND_ADDBANS);
   if (msg())
   {
      (void) msg()->AddString(PR_NAME_KEYS, arg);
      (void) gw.AddOutgoingMessage(msg);
      if (unBan) LogTime(MUSCLE_LOG_INFO, "Removing ban patterns that match pattern [%s]\n", arg);
            else LogTime(MUSCLE_LOG_INFO, "Adding ban pattern [%s]\n", arg);
   }
}

static void Require(MessageIOGateway & gw, const char * arg, bool unRequire);
void Require(MessageIOGateway & gw, const char * arg, bool unRequire)
{
   MessageRef msg = GetMessageFromPool(unRequire ? PR_COMMAND_REMOVEREQUIRES : PR_COMMAND_ADDREQUIRES);
   if (msg())
   {
      (void) msg()->AddString(PR_NAME_KEYS, arg);
      (void) gw.AddOutgoingMessage(msg);
      if (unRequire) LogTime(MUSCLE_LOG_INFO, "Removing require patterns that match pattern [%s]\n", arg);
            else LogTime(MUSCLE_LOG_INFO, "Adding require pattern [%s]\n", arg);
   }
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
   const int16 port = cln ? atoi(cln+1) : 2960;

   ConstSocketRef s = Connect(String(hostName).Substring(0, ":")(), port, "admin", false);
   if (s() == NULL)
   {
      LogTime(MUSCLE_LOG_INFO, "(run 'admin help' for arguments)\n");
      return 10;
   }

   MessageIOGateway gw;
   gw.SetDataIO(DataIORef(newnothrow TCPSocketDataIO(s, false)));
   if (gw.GetDataIO()() == NULL) return 10;

   // Now generate all commands...
   for (int j=1; j<argc; j++)
   {
      char * line = argv[j];
      char * colon = strchr(line, '=');
      if (colon)
      {
         char * arg = colon+1;
         *colon = '\0';

              if (strcmp(line, "kick")      == 0) Kick(gw, arg);
         else if (strcmp(line, "ban")       == 0) Ban(gw, arg, false);
         else if (strcmp(line, "unban")     == 0) Ban(gw, arg, true);
         else if (strcmp(line, "require")   == 0) Require(gw, arg, false);
         else if (strcmp(line, "unrequire") == 0) Require(gw, arg, true);
         else if (strcmp(line, "kickban")   == 0)
         {
            Kick(gw, arg);
            Ban(gw, arg, false);
         }
      }
   }

   // Lastly, request a PONG so we know when all has been done
   (void) gw.AddOutgoingMessage(MessageRef(GetMessageFromPool(PR_COMMAND_PING)));

   // send them, and then wait for the pong back
   uint32 errorCount = 0;
   QueueGatewayMessageReceiver inQueue;
   SocketMultiplexer multiplexer;
   while(s())
   {
      const int fd = s()->GetFileDescriptor();
      multiplexer.RegisterSocketForReadReady(fd);
      if (gw.HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);

      if (multiplexer.WaitForEvents() < 0)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "WaitForEvents() failed, exiting! [%s]\n", B_ERRNO());
         errorCount++;
         break;
      }

      const bool readError  = ((multiplexer.IsSocketReadyForRead(fd)) &&(gw.DoInput(inQueue) < 0));
      const bool writeError = ((multiplexer.IsSocketReadyForWrite(fd))&&(gw.DoOutput()       < 0));
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
                  MessageRef subMsg;
                  LogTime(MUSCLE_LOG_ERROR, "Access denied!  ");
                  const char * who;
                  if ((msg->FindMessage(PR_NAME_REJECTED_MESSAGE, subMsg).IsOK())&&(subMsg()->FindString(PR_NAME_KEYS, &who).IsOK()))
                  {
                     const char * action = "do that to";
                     switch(subMsg() ? subMsg()->what : 0)
                     {
                        case PR_COMMAND_KICK:           action = "kick";      break;
                        case PR_COMMAND_ADDBANS:        action = "ban";       break;
                        case PR_COMMAND_REMOVEBANS:     action = "unban";     break;
                        case PR_COMMAND_ADDREQUIRES:    action = "require";   break;
                        case PR_COMMAND_REMOVEREQUIRES: action = "unrequire"; break;
                     }
                     Log(MUSCLE_LOG_ERROR, "You are not allowed to %s [%s]!", action, who);
                  }
                  Log(MUSCLE_LOG_ERROR, "\n");
               }
               break;
            }
         }
      }     
   }
   LogTime(MUSCLE_LOG_INFO, "Exiting. (" UINT32_FORMAT_SPEC " errors)\n", errorCount);
   return 0;
}
