#ifndef MuscleCPULoadMeter_h 
#define MuscleCPULoadMeter_h 

#include "support/MuscleSupport.h"
#include "util/CountedObject.h"

namespace muscle {

/** This class knows how to measure the total load on the host computer's CPU.
  * Note that the internal implementation of this class is OS-specific, and so
  * it will only work properly on the OS's for which an implementation has been
  * provided (currently Windows, MacOS/X, and Linux).  Under other OS's, 
  * GetCPULoad() will always just return a negative value.
  *
  * To use this class, just instantiate a CPULoadMeter object, and then call
  * GetCPULoad() every so often (e.g. whenever you want to update your CPU load display)
  */
class CPULoadMeter : private CountedObject<CPULoadMeter>
{
public:
   /** Default constructor */
   CPULoadMeter();

   /** Destructor. */
   ~CPULoadMeter();

   /** Returns the percentage CPU load, measured since the last time this method was called.
     * @note Currently this method is implemented only for Linux, OS/X, and Windows.
     *       For other operating systems, this method will always return a negative value.
     * @returns 0.0f if the CPU was idle, 1.0f if the CPU was fully loaded, or something
     *          in between.  Returns a negative value if the CPU time could not be measured.
     */
   float GetCPULoad();

private:
   float CalculateCPULoad(uint64 idleTicks, uint64 totalTicks); 

   uint64 _previousTotalTicks;
   uint64 _previousIdleTicks;

#ifdef WIN32
# if defined(MUSCLE_USING_NEW_MICROSOFT_COMPILER)
// we will use the statically linked version
# else
#  define USE_KERNEL32_DLL_FOR_GETSYSTEMTIMES 1
# endif
#endif

#ifdef USE_KERNEL32_DLL_FOR_GETSYSTEMTIMES
   typedef WINBASEAPI BOOL WINAPI (*GetSystemTimesProc) (OUT LPFILETIME t1, OUT LPFILETIME t2, OUT LPFILETIME t3);
   GetSystemTimesProc _getSystemTimesProc;
   HMODULE _winKernelLib;
#endif
};

}; // end namespace muscle

#endif
