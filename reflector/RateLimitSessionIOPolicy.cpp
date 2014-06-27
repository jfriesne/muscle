/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "reflector/RateLimitSessionIOPolicy.h"

namespace muscle {

#define CUTOFF (_byteLimit/2)

RateLimitSessionIOPolicy :: 
RateLimitSessionIOPolicy(uint32 maxRate, uint32 primeBytes) : _maxRate(maxRate), _byteLimit(primeBytes), _lastTransferAt(0), _transferTally(0)
{
   // empty
}

RateLimitSessionIOPolicy :: 
~RateLimitSessionIOPolicy()
{
   // empty
}

void 
RateLimitSessionIOPolicy :: 
PolicyHolderAdded(const PolicyHolder &)
{
   // empty
}

void 
RateLimitSessionIOPolicy :: 
PolicyHolderRemoved(const PolicyHolder &)
{
   // empty
}

void 
RateLimitSessionIOPolicy :: 
BeginIO(uint64 now)
{
   UpdateTransferTally(now);

   _lastTransferAt  = now;
   _numParticipants = 0;

   // If we aren't going to allow anyone to transfer,
   // we'll need to make sure the server wakes up so
   // we can do transfers later, after some time has passed.
   if (_transferTally>=CUTOFF) InvalidatePulseTime();
}

uint64
RateLimitSessionIOPolicy ::
GetPulseTime(const PulseArgs & args)
{
   // Schedule a pulse for when we estimate _transferTally will sink back down to zero.  
   return ((_maxRate > 0)&&(_transferTally>=CUTOFF))?(args.GetCallbackTime()+((_transferTally*MICROS_PER_SECOND)/_maxRate)):MUSCLE_TIME_NEVER;
}

void
RateLimitSessionIOPolicy ::
Pulse(const PulseArgs & args)
{
   TCHECKPOINT;
   UpdateTransferTally(args.GetCallbackTime());
}

void
RateLimitSessionIOPolicy ::
UpdateTransferTally(uint64 now)
{
   if (_maxRate > 0)
   {
      uint32 newBytesAvailable = (_lastTransferAt > 0) ? ((uint32)(((now-_lastTransferAt)*_maxRate)/MICROS_PER_SECOND)) : MUSCLE_NO_LIMIT;
      if (_transferTally > newBytesAvailable) _transferTally -= newBytesAvailable;
                                         else _transferTally = 0;
   }
   else _transferTally = MUSCLE_NO_LIMIT;  // disable all writing by pretending we just wrote a whole lot
}

bool 
RateLimitSessionIOPolicy ::
OkayToTransfer(const PolicyHolder &)
{
   if ((_maxRate > 0)&&(_transferTally < CUTOFF))
   {
      _numParticipants++;
      return true;
   }
   else return false;
}

uint32 
RateLimitSessionIOPolicy :: 
GetMaxTransferChunkSize(const PolicyHolder &)
{
   MASSERT(_numParticipants>0, "RateLimitSessionIOPolicy::GetMax: no participants!?!?");
   return (_transferTally<_byteLimit)?((_byteLimit-_transferTally)/_numParticipants):0;
}

void 
RateLimitSessionIOPolicy :: 
BytesTransferred(const PolicyHolder &, uint32 numBytes)
{
   _transferTally += numBytes;
}

void 
RateLimitSessionIOPolicy :: 
EndIO(uint64)
{
   // empty
}

}; // end namespace muscle
