/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef RateLimitSessionIOPolicy_h
#define RateLimitSessionIOPolicy_h

#include "reflector/AbstractSessionIOPolicy.h"

namespace muscle {

/** 
 * This policy allows you to enforce an aggregate maximum bandwidth usage for the set
 * of AbstractReflectSessionSessions that use it.  Each policy object may referenced by
 * zero or more PolicyHolders at once.
 */
class RateLimitSessionIOPolicy : public AbstractSessionIOPolicy, private CountedObject<RateLimitSessionIOPolicy>
{
public:
   /** Constructor.  
     * @param maxRate The maximum aggregate transfer rate to be enforced for all sessions
     *                that use this policy, in bytes per second.
     * @param primeBytes When the bytes first start to flow, the policy allows the first (primeBytes)
     *                   bytes to be sent out immediately, before clamping down on the flow rate.
     *                   This helps keep the policy from having to wake up the server too often,
     *                   and saves CPU time.  This parameter lets you adjust that startup-size.
     *                   Defaults to 2048 bytes.
     */
   RateLimitSessionIOPolicy(uint32 maxRate, uint32 primeBytes = 2048);

   /** Destructor. */
   virtual ~RateLimitSessionIOPolicy();

   virtual void PolicyHolderAdded(const PolicyHolder & holder);
   virtual void PolicyHolderRemoved(const PolicyHolder & holder);

   virtual void BeginIO(uint64 now);
   virtual bool OkayToTransfer(const PolicyHolder & holder);
   virtual uint32 GetMaxTransferChunkSize(const PolicyHolder & holder);
   virtual void BytesTransferred(const PolicyHolder & holder, uint32 numBytes);
   virtual void EndIO(uint64 now);

   virtual uint64 GetPulseTime(const PulseArgs & args);
   virtual void Pulse(const PulseArgs & args);

private:
   void UpdateTransferTally(uint64 now);

   uint32 _maxRate;
   uint32 _byteLimit;
   uint64 _lastTransferAt;
   uint32 _transferTally;
   uint32 _numParticipants;
};

}; // end namespace muscle

#endif
