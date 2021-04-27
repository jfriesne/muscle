/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#ifdef __ATHEOS__
# include <util/looper.h>
# define BLooper os::Looper
# define BMessage os::Message
# define BMessenger os::Messenger
#else
# include <app/Looper.h>
#endif

#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"

#ifdef __ATHEOS__
# include "atheossupport/AThread.h"
# include "atheossupport/ConvertMessages.h"
# define BMessageTransceiverThread AMessageTransceiverThread
#elif defined(__BEOS__) || defined(__HAIKU__)
# include "besupport/BThread.h"
# include "besupport/ConvertMessages.h"
#else
# error "testreflectclient only works under BeOS, Haiku, and AtheOS.  Try portablereflectclient instead!
#endif

#include "util/NetworkUtilityFunctions.h"
#include "util/StringTokenizer.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Test failed, line %i\n",__LINE__)

enum {
   METHOD_MANUAL = 0,
   METHOD_AUTOMATIC,
   METHOD_ACCEPT
};

// This BeOS-specific program is used to test the muscled server.  It is more-or-less
// functionally equivalent to the portablereflectclient, but exercises the BeOS-specific
// functionality found in the besupport class as well. 
class MyLooper : public BLooper
{
public:
   explicit MyLooper(MessageTransceiverThread & mtt)
      : 
#ifdef __ATHEOS__
     Looper(""),
#endif     
   _transThread(mtt) {}

#ifdef __ATHEOS__
   virtual void HandleMessage(os::Message * msg)
#else
   virtual void MessageReceived(BMessage * msg)
#endif
   {
#ifdef __ATHEOS__
      switch(msg->GetCode())
#else      
      switch(msg->what)
#endif
      {
         case MUSCLE_THREAD_SIGNAL:
         {
            uint32 code;
            MessageRef ref; 
            String session;
            uint32 factoryID;
            IPAddressAndPort location;
            while(_transThread.GetNextEventFromInternalThread(code, &ref, &session, &factoryID, &location) >= 0)
            {
               String codeStr;
               switch(code)
               {
                  case MTT_EVENT_INCOMING_MESSAGE:      codeStr = "IncomingMessage";     break;
                  case MTT_EVENT_SESSION_ACCEPTED:      codeStr = "SessionAccepted";     break;
                  case MTT_EVENT_SESSION_ATTACHED:      codeStr = "SessionAttached";     break;
                  case MTT_EVENT_SESSION_CONNECTED:     codeStr = "SessionConnected";    break;
                  case MTT_EVENT_SESSION_DISCONNECTED:  codeStr = "SessionDisconnected"; break;
                  case MTT_EVENT_SESSION_DETACHED:      codeStr = "SessionDetached";     break;
                  case MTT_EVENT_FACTORY_ATTACHED:      codeStr = "FactoryAttached";     break;
                  case MTT_EVENT_FACTORY_DETACHED:      codeStr = "FactoryDetached";     break;
                  case MTT_EVENT_OUTPUT_QUEUES_DRAINED: codeStr = "OutputQueuesDrained"; break;
                  case MTT_EVENT_SERVER_EXITED:         codeStr = "ServerExited";        break;
                  default:
                  {
                     char buf[5]; MakePrettyTypeCodeString(code, buf);
                     codeStr = "\'";
                     codeStr += buf;
                     codeStr += "\'";
                  }
               }
               printf("/------------------------------------------------------------\n");
               printf("Event from MTT:  type=[%s], session=[%s] factoryID=[" UINT32_FORMAT_SPEC "] location=[%s]\n", codeStr(), session(), factoryID, location.ToString()());
               if (ref()) ref()->PrintToStream();
               printf("\\------------------------------------------------------------\n");
            }
         }
         break;

         default:
            printf("MyLooper:  Received unknown BMessage:\n");
#ifndef __ATHEOS__
            msg->PrintToStream();
#endif
         break;
      }
   }

   MessageTransceiverThread & _transThread;
};

status_t SetupTransceiverThread(MessageTransceiverThread & mtt, const char * hostName, uint16 port, int method)
{
   status_t ret;
   switch(method)
   {
      case METHOD_MANUAL:
         if (mtt.AddNewSession(Connect(hostName, port, "trc")).IsError(ret))
         {
            printf("Error adding manual session!\n");
            return ret;
         }
      break;

      case METHOD_AUTOMATIC:
         if (mtt.AddNewConnectSession(hostName, port).IsError(ret))
         {
            printf("Error adding connect session!\n");
            return ret;
         }
         else printf("Will connect asynchronously to %s\n", GetConnectString(hostName, port)());
      break;

      case METHOD_ACCEPT:
      {
         if (mtt.PutAcceptFactory(port).IsError(ret))
         {
            printf("Error adding accept factory!\n");
            return ret;
         }
         printf("Accepting connections\n");
      }
      break;

      default:
         printf("CreateTransceiverThread(): unknown method %i\n", method);
         return B_BAD_ARGUMENT;
      break;
   }

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   (void) mtt.SetOutgoingMessageEncoding(MUSCLE_MESSAGE_ENCODING_ZLIB_1);

   MessageRef zlibRef = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
   if (zlibRef())
   {
      zlibRef()->AddInt32(PR_NAME_REPLY_ENCODING, MUSCLE_MESSAGE_ENCODING_ZLIB_1);
      mtt.SendMessageToSessions(zlibRef);
   }
#endif

   return B_NO_ERROR;
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   char * hostName = "localhost";
   int port = 0;

   int method = -1;  
   if (argc > 1) 
   {
      if (strcmp(argv[1], "-connect") == 0)
      {
         method = METHOD_AUTOMATIC;
         if (argc > 2) hostName = argv[2];
         if (argc > 3) port = muscleMax(0, atoi(argv[3]));
      } 
      else if (strcmp(argv[1], "-connectsync") == 0)
      {
         method = METHOD_MANUAL;
         if (argc > 2) hostName = argv[2];
         if (argc > 3) port = muscleMax(0, atoi(argv[3]));
      }
      else if (strcmp(argv[1], "-accept") == 0)
      {
         method = METHOD_ACCEPT;
         if (argc > 2) port = muscleMax(0, atoi(argv[2]));
      }
   }
   if (port == 0) port = 2960;

   if (method != -1)
   {
      BMessageTransceiverThread mtt;
      MyLooper * looper = new MyLooper(mtt);
      looper->Run();
      mtt.SetTarget(looper);
      if ((mtt.StartInternalThread().IsOK())&&(SetupTransceiverThread(mtt, hostName, (uint16)port, method).IsOK()))
      {
         char text[1000];
         bool keepGoing = true;
         while(keepGoing)
         {
            if (fgets(test, sizeof(test), stdin) == NULL) test[0] = '\0';
            printf("You typed: [%s]\n",text);
            if (*text != '\0')
            {
               bool send = true;
               MessageRef ref = GetMessageFromPool();

               switch(text[0])
               {
                  case 'r': 
                     printf("Requesting output-queues-drained notification\n");
                     mtt.RequestOutputQueuesDrainedNotification(MessageRef());
                     send = false;
                  break;

                  case 'f':
                  {
                     // Test the server's un-spoof-ability
                     ref()->what = 'HELO';
                     ref()->AddString(PR_NAME_SESSION, "nerdboy");  // should be changed transparently by the server
                  }
                  break;
   
                  case 'i':
                  {
                     StringTokenizer tok(&text[2]);
                     const char * node   = tok.GetNextToken();
                     const char * before = tok.GetNextToken();
                     const char * value  = tok.GetNextToken();
                     printf("Insert [%s] before [%s] under [%s]\n", value, before, node);
                     ref()->what = PR_COMMAND_INSERTORDEREDDATA;
                     ref()->AddString(PR_NAME_KEYS, node);
                     Message child(atoi(value));
                     child.AddString("wtf?", value);
                     ref()->AddMessage(before, child);
                  }
                  break;

                  case 'm':
                     ref()->what = 'umsg';
                     if (text[1]) ref()->AddString(PR_NAME_KEYS, &text[2]);
                     ref()->AddString("info", text[1] ? "This is a directed user message" : "This is a default-directed user message");
                  break;

                  case 'M':
                     ref()->what = PR_COMMAND_SETPARAMETERS;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 's':
                  {
                     ref()->what = PR_COMMAND_SETDATA;
                     MessageRef dataMsg = GetMessageFromPool('HELO');
                     if (dataMsg())
                     {
                        dataMsg()->AddInt32("val", atol(&text[2]));
                        ref()->AddMessage(&text[2], dataMsg);
                     }
                  }
                  break;

                  case 'g':
                     ref()->what = PR_COMMAND_GETDATA;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'k':
                     ref()->what = PR_COMMAND_KICK;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'b':
                     ref()->what = PR_COMMAND_ADDBANS;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'B':
                     ref()->what = PR_COMMAND_REMOVEBANS;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;             

                  case 'G':
                     ref()->what = PR_COMMAND_GETDATA;
                     ref()->AddString(PR_NAME_KEYS, "j*/k*");
                     ref()->AddString(PR_NAME_KEYS, "k*/j*");
                  break;

                  case 'q':
                     keepGoing = send = false;
                  break;

                  // test undefined type
                  case 'u':
                  {
                     const uint8 junk[] = "junkman";
                     ref()->AddData("junk type", 0x1234, (const void *) junk, sizeof(junk));
                     ref()->PrintToStream();
                  }
                  break;

                  case 'x':
                     ref()->what = PR_COMMAND_SETPARAMETERS;
                     (void) ref()->AddArchiveMessage(&text[2], Int32QueryFilter("val", Int32QueryFilter::OP_GREATER_THAN, 10));
                  break;

                  case 'p':
                     ref()->what = PR_COMMAND_SETPARAMETERS;
                     ref()->AddString(&text[2], "");
                  break;

                  case 'P':
                     ref()->what = PR_COMMAND_GETPARAMETERS;
                  break;

                  case 'd':
                     ref()->what = PR_COMMAND_REMOVEDATA;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'D':
                     ref()->what = PR_COMMAND_REMOVEPARAMETERS;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'I':
                     ref()->what = 1651666225;
                     ref()->AddString("br_authorid", "jeremy");
                     ref()->AddString("br_streamid", "smurfs");
                     ref()->AddInt32("br_hostip", GetHostByName("beshare.befaqs.com", false));
                     ref()->AddInt16("br_port", 2960);
                  break;

                  case 't':
                  {
                     // test all data types
                     ref()->what = 1234;
                     ref()->AddString("String", "this is a string");
                     ref()->AddInt8("Int8", 123);
                     ref()->AddInt8("-Int8", -123);
                     ref()->AddInt16("Int16", 1234);
                     ref()->AddInt16("-Int16", -1234);
                     ref()->AddInt32("Int32", 12345);
                     ref()->AddInt32("-Int32", -12345);
                     ref()->AddInt64("xInt64", -1);
                     ref()->AddInt64("xInt64", 1);
                     ref()->AddInt64("Int64", 123456789);
                     ref()->AddInt64("-Int64", -123456789);
                     ref()->AddBool("Bool", true);
                     ref()->AddBool("-Bool", false);
                     ref()->AddFloat("Float", 1234.56789f);
                     ref()->AddFloat("-Float", -1234.56789f);
                     ref()->AddDouble("Double", 1234.56789);
                     ref()->AddDouble("-Double", -1234.56789);
                     ref()->AddPointer("Pointer", ref());
                     ref()->AddRect("Rect", Rect(1,2,3,4));
                     ref()->AddRect("Rect", Rect(2,3,4,5));
                     ref()->AddPoint("Point", Point(4,5));
                     ref()->AddFlat("Flat", *ref());
                     char data[] = "This is some data";
                     ref()->AddData("Flat2", B_RAW_TYPE, data, sizeof(data));
                  }
                  break;

                  default:
                     printf("Sorry, wot?\n");
                     send = false;
                  break;
               }

               if (send) 
               {
                  printf("Sending message...\n");
//                  ref()->PrintToStream();
                  mtt.SendMessageToSessions(ref);
               }
            }
         }

         printf("Shutting down MessageTransceiverThread...\n");
         mtt.ShutdownInternalThread();

         printf("Shutting down looper...\n");
         if (looper->Lock()) looper->Quit();
      }
      else printf("Could not set up session!\n");
   }
   else
   {
      printf("Usage:  testreflectclient -connect [hostname=localhost] [port=2960]\n");
      printf("        testreflectclient -connectsync [hostname=localhost] [port=2960]\n");
      printf("        testreflectclient -accept [port=0]\n");
   }
   printf("Test client exiting, bye!\n");
   return 0;
}

