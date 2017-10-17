#ifndef INetworkConfigChangesTarget_h
#define INetworkConfigChangesTarget_h

#include "util/String.h"   // to avoid a static_assert error below, when MUSCLE_AVOID_CPLUSPLUS11 is not defined
#include "util/Hashtable.h"

namespace muscle {

/** This is an abstract base class (interface) that can be inherited by any object that 
  * wants the DetectNetworkConfigChangesSession to notify it when one or more
  * network interfaces on the local computer have changed, or when the host computer
  * is about to go to sleep or wake up.
  */
class INetworkConfigChangesTarget
{
public:
   /** Default constructor */
   INetworkConfigChangesTarget() {/* empty */}

   /** Destructor */
   virtual ~INetworkConfigChangesTarget() {/* empty */}

   /** Called by the DetectNetworkConfigChanges session's default NetworkInterfacesChanged()
     * method after the set of local network interfaces has changed.
     * @param optInterfaceNames optional table containing the names of the interfaces that
     *                          have changed (e.g. "en0", "en1", etc).  If this table is empty,
     *                          that indicates that any or all of the network interfaces may have
     *                          changed.  Note that changed-interface enumeration is currently only
     *                          implemented under MacOS/X and Windows, so under other operating systems this
     *                          argument will currently always be an empty table.
     */
   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & optInterfaceNames) = 0;

   /** Called by the DetectNetworkConfigChanges session, when the host computer is about to go to sleep.  
     * Currently implemented for Windows and MacOS/X only.  
     */
   virtual void ComputerIsAboutToSleep() = 0;

   /** Called by the DetectNetworkConfigChanges session, when the host computer has just woken up from sleep.  
     * Currently implemented for Windows and MacOS/X only.
     */
   virtual void ComputerJustWokeUp() = 0;
};

}  // end namespace muscle

#endif
