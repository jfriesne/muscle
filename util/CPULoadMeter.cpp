#include "util/CPULoadMeter.h"
#include "util/StringTokenizer.h"

#ifdef __APPLE__
# include <mach/mach_init.h>
# include <mach/mach_error.h>
# include <mach/mach_host.h>
# include <mach/vm_map.h>
#endif

namespace muscle {

#ifdef WIN32
# ifndef USE_KERNEL32_DLL_FOR_GETSYSTEMTIMES
#  include <Windows.h>
#  include <winbase.h>
# endif
static uint64 FileTimeToInt64(const FILETIME & ft) {return (((uint64)(ft.dwHighDateTime))<<32)|((uint64)ft.dwLowDateTime);}
#endif

CPULoadMeter :: CPULoadMeter() : _previousTotalTicks(0), _previousIdleTicks(0)
{
#ifdef USE_KERNEL32_DLL_FOR_GETSYSTEMTIMES
   // Gotta dynamically load this system call, because the Borland headers doesn't know about it.  :^P
   _winKernelLib = LoadLibrary(TEXT("kernel32.dll"));
   if (_winKernelLib) _getSystemTimesProc = (GetSystemTimesProc) GetProcAddress(_winKernelLib, "GetSystemTimes");
#endif
}

CPULoadMeter :: ~CPULoadMeter()
{
#ifdef USE_KERNEL32_DLL_FOR_GETSYSTEMTIMES
   if (_winKernelLib != NULL) FreeLibrary(_winKernelLib);
#endif
}

float CPULoadMeter :: CalculateCPULoad(uint64 idleTicks, uint64 totalTicks)
{
   uint64 totalTicksSinceLastTime = totalTicks-_previousTotalTicks;
   uint64 idleTicksSinceLastTime  = idleTicks-_previousIdleTicks;
   float ret = 1.0f-((totalTicksSinceLastTime > 0) ? ((float)idleTicksSinceLastTime)/totalTicksSinceLastTime : 0);
   _previousTotalTicks = totalTicks;
   _previousIdleTicks  = idleTicks;
   return ret;
}

float CPULoadMeter :: GetCPULoad()
{
   TCHECKPOINT;

   float sysLoadPercentage = -1.0f;  // default (aka unset)

#ifdef __linux__
   FILE * fpIn = fopen("/proc/stat", "r");
   if (fpIn)
   {
      char buf[1024];
      while(fgets(buf, sizeof(buf), fpIn))
      {
         if (strncmp(buf, "cpu ", 4) == 0)
         {
            StringTokenizer tok(false, &buf[4]); 
            const char *                                  next = tok();
            uint64 userTicks   = next ? Atoull(next) : 0; next = tok();
            uint64 niceTicks   = next ? Atoull(next) : 0; next = tok();
            uint64 systemTicks = next ? Atoull(next) : 0; next = tok();
            uint64 idleTicks   = next ? Atoull(next) : 0;
            sysLoadPercentage = CalculateCPULoad(idleTicks, userTicks+niceTicks+systemTicks+idleTicks);
            break;
         }
      }
      fclose(fpIn);
   }
#elif WIN32
# ifdef USE_KERNEL32_DLL_FOR_GETSYSTEMTIMES
   if (_getSystemTimesProc)
   {
      FILETIME idleTime, kernelTime, userTime;
      if (_getSystemTimesProc(&idleTime, &kernelTime, &userTime)) sysLoadPercentage = CalculateCPULoad(FileTimeToInt64(idleTime), FileTimeToInt64(kernelTime)+FileTimeToInt64(userTime));
   }
# else
   {  // keep these variables local
      FILETIME idleTime, kernelTime, userTime;
      if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) sysLoadPercentage = CalculateCPULoad(FileTimeToInt64(idleTime), FileTimeToInt64(kernelTime)+FileTimeToInt64(userTime));
   }
# endif
#elif __APPLE__
   host_cpu_load_info_data_t cpuinfo;
   mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
   if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuinfo, &count) == KERN_SUCCESS)
   {
      uint64 totalTicks = 0;
      for(int i=0; i<CPU_STATE_MAX; i++) totalTicks += cpuinfo.cpu_ticks[i];
      sysLoadPercentage = CalculateCPULoad(cpuinfo.cpu_ticks[CPU_STATE_IDLE], totalTicks);
   }
#endif

   return sysLoadPercentage;
}

}; // end namespace muscle
