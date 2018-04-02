#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Socket.h"
#include "util/NetworkUtilityFunctions.h"  // for CreateUDPSocket()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the muscle::Socket class and ConstSocketRef\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Atypical usage:  Capturing a file descriptor into a Socket object so that
   // it will be automatically close()'d when the execution leaves the enclosing scope
   {
      int some_fd = socket(AF_INET, SOCK_DGRAM, 0);
      Socket mySock(some_fd);
      // [...]
      // close(some_fd) will automatically be called here by the Socket object's destructor 
   }

   // Still atypical, but this time we'll use a ConstSocketRef object so that we can 
   // keep the file descriptor valid outside of the scope it was created in.
   {
      ConstSocketRef sockRef;

      {
         ConstSocketRef tempRef = GetConstSocketRefFromPool(socket(AF_INET, SOCK_DGRAM, 0));
         sockRef = tempRef;  // Share the reference outside our inner-local-scope
         // Note that the socket is NOT close()'d here, because sockRef still references it
      }
      // [...]
      // the socket IS closed here, because the last ConstSocketRef pointing to it (sockRef) is destroyed here
   }

   // Here's a more typical usage, via the NetworkUtilityFunctions.h API
   {
      ConstSocketRef sockRef = CreateUDPSocket();  // returns a ready-to-use UDP socket
      if (sockRef())
      {
         printf("Allocated UDP socket ref:  ConstSocketRef=%p, underlying fd is %i\n", sockRef(), sockRef.GetFileDescriptor());

         // Code using the UDP socket could go here
         // [...]
         // UDP socket gets close()'d here 
      }
      else printf("Failed to create the UDP socket!?\n");
   }

   return 0;
}
