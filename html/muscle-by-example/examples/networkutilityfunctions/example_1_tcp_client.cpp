#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/NetworkUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates some basic usage of the NetworkUtilityFunctions API by downloading the search-page HTML from www.google.com and printing it to stdout.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();
   
   LogTime(MUSCLE_LOG_INFO, "Downloading the HTML data from www.google.com port 80...\n");

   ConstSocketRef tcpSock = Connect("www.google.com", 80, "TCP Client", false, SecondsToMicros(10));
   if (tcpSock())
   {
      // Send HTTP request out to the Google
      const char httpRequestStr[] = "GET /index.html HTTP/1.0\r\n\r\n";
      const int reqLen = strlen(httpRequestStr);
      int numBytesSent = WriteData(tcpSock, httpRequestStr, reqLen, true);
      LogTime(MUSCLE_LOG_INFO, "%i/%i bytes of HTTP request data sent to the server.\n", numBytesSent, reqLen);

      // Read back the server's response data and print it to stdout
      char buf[2048];
      int numBytesRead;
      while((numBytesRead = ReadData(tcpSock, buf, sizeof(buf)-1, true)) >= 0)
      {
         buf[numBytesRead] = '\0';  // make sure our received data is NUL-terminated before we try to print it
         LogTime(MUSCLE_LOG_INFO, "Received %i bytes: [%s]\n", numBytesRead, buf);
      }

      LogTime(MUSCLE_LOG_INFO, "TCP connection closed\n");
   }
   else LogTime(MUSCLE_LOG_INFO, "TCP connection failed!\n");

   LogTime(MUSCLE_LOG_INFO, "\n");
   return 0;
}
