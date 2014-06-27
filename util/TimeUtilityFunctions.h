/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleTimeUtilityFunctions_h
#define MuscleTimeUtilityFunctions_h

#include <time.h>

#include "support/MuscleSupport.h"
#include "syslog/SysLog.h"  // for MUSCLE_TIMEZONE_*

#ifndef WIN32
# include <sys/time.h>
#endif

#if defined(__BEOS__) || defined(__HAIKU__)
# include <kernel/OS.h>
#endif

#if defined(TARGET_PLATFORM_XENOMAI)
# include "native/timer.h"
#endif

namespace muscle {

/** @defgroup timeutilityfunctions The TimeUtilityFunctions function API
 *  These functions are all defined in TimeUtilityFunctions.h, and are stand-alone
 *  inline functions that do various time-related calculations
 *  @{
 */

/** A value that GetPulseTime() can return to indicate that Pulse() should never be called. */
#define MUSCLE_TIME_NEVER ((uint64)-1) // (9223372036854775807LL)

/** How many milliseconds are in one second (1000) */
#define MILLIS_PER_SECOND  ((int64)1000)

/** How many microseconds are in one second (1000000) */
#define MICROS_PER_SECOND  ((int64)1000000)

/** How many nanoseconds are in one second (1000000000) */
#define NANOS_PER_SECOND   ((int64)1000000000)

/** Given a value in seconds, returns the equivalent number of nanoseconds. 
  * @param s A time value, in seconds.
  */
inline int64 SecondsToNanos(int64 s)     {return s*NANOS_PER_SECOND;}

/** Given a value in seconds, returns the equivalent number of microseconds. 
  * @param s A time value, in seconds.
  */
inline int64 SecondsToMicros(int64 s)    {return s*MICROS_PER_SECOND;}

/** Given a value in seconds, returns the equivalent number of milliseconds. 
  * @param s A time value, in seconds.
  */
inline int64 SecondsToMillis(int64 s)    {return s*MILLIS_PER_SECOND;}

/** Given a value in milliseconds, returns the equivalent number of nanoseconds. 
  * @param ms A time value, in milliseconds.
  */
inline int64 MillisToNanos(int64 ms)     {return ms*(1000*1000);}

/** Given a value in milliseconds, returns the equivalent number of nanoseconds. 
  * @param ms A time value, in milliseconds.
  */
inline int64 MillisToMicros(int64 ms)    {return ms*1000;}

/** Given a value in milliseconds, returns the equivalent number of nanoseconds. 
  * @param ms A time value, in milliseconds.
  */
inline int64 MillisToSeconds(int64 ms)   {return ms/MILLIS_PER_SECOND;}

/** Given a value in microseconds, returns the equivalent number of nanoseconds. 
  * @param us A time value, in microseconds.
  */
inline int64 MicrosToNanos(int64 us)     {return us*1000;}

/** Given a value in microseconds, returns the equivalent number of milliseconds. 
  * @param us A time value, in microseconds.
  */
inline int64 MicrosToMillis(int64 us)    {return us/1000;}

/** Given a value in microseconds, returns the equivalent number of seconds. 
  * @param us A time value, in microseconds.
  */
inline int64 MicrosToSeconds(int64 us)   {return us/MICROS_PER_SECOND;}

/** Given a value in nanoseconds, returns the equivalent number of microseconds. 
  * @param ns A time value, in nanoseconds.
  */
inline int64 NanosToMicros(int64 ns)     {return ns/1000;}

/** Given a value in nanoseconds, returns the equivalent number of milliseconds. 
  * @param ns A time value, in nanoseconds.
  */
inline int64 NanosToMillis(int64 ns)     {return ns/(1000*1000);}

/** Given a value in nanoseconds, returns the equivalent number of seconds. 
  * @param ns A time value, in nanoseconds.
  */
inline int64 NanosToSeconds(int64 ns)    {return ns/NANOS_PER_SECOND;}

/** Given a value in minutes, returns the equivalent number of microseconds. 
  * @param m A time value, in minutes.
  */
inline int64 MinutesToMicros(int64 m)    {return SecondsToMicros(m*60);}

/** Given a value in hours, returns the equivalent number of microseconds. 
  * @param h A time value, in hours.
  */
inline int64 HoursToMicros(int64 h)      {return MinutesToMicros(h*60);}

/** Given a value in days, returns the equivalent number of microseconds. 
  * @param d A time value, in days.
  */
inline int64 DaysToMicros(int64 d)       {return HoursToMicros(d*24);}

/** Given a value in weeks, returns the equivalent number of microseconds. 
  * @param w A time value, in weeks.
  */
inline int64 WeeksToMicros(int64 w)      {return DaysToMicros(w*7);}

/** Given a timeval struct, returns the equivalent uint64 value (in microseconds).
 *  @param tv a timeval to convert
 *  @return A uint64 representing the same time.
 */
inline uint64 ConvertTimeValTo64(const struct timeval & tv) {return SecondsToMicros(tv.tv_sec) + ((uint64)tv.tv_usec);}

/** Given a uint64, writes the equivalent timeval struct into the second argument.
 *  @param val A uint64 time value in microseconds
 *  @param retStruct On return, contains the equivalent timeval struct to (val)
 */
inline void Convert64ToTimeVal(uint64 val, struct timeval & retStruct)
{
   retStruct.tv_sec  = (int32)(val / MICROS_PER_SECOND);
   retStruct.tv_usec = (int32)(val % MICROS_PER_SECOND);
}

/** Convenience function:  Returns true true iff (t1 < t2)
 *  @param t1 a time value
 *  @param t2 another time value
 *  @result true iff t1 < t2
 */
inline bool IsLessThan(const struct timeval & t1, const struct timeval & t2)
{
   return (t1.tv_sec == t2.tv_sec) ? (t1.tv_usec < t2.tv_usec) : (t1.tv_sec < t2.tv_sec);
}

/** Convenience function: Adds (addThis) to (addToThis)
 *  @param addToThis A time value.  On return, this time value will have been modified.
 *  @param addThis Another time value, that will be added to (addToThis).
 */
inline void AddTimeVal(struct timeval & addToThis, const struct timeval & addThis)
{
   addToThis.tv_sec  += addThis.tv_sec;
   addToThis.tv_usec += addThis.tv_usec;
   if (addToThis.tv_usec > MICROS_PER_SECOND)
   {
      addToThis.tv_sec += addToThis.tv_usec / MICROS_PER_SECOND;
      addToThis.tv_usec = addToThis.tv_usec % MICROS_PER_SECOND;
   }
}

/** Convenience function: Subtracts (subtractThis) from (subtractFromThis)
 *  @param subtractFromThis A time value.  On return, this time value will have been modified.
 *  @param subtractThis Another time value, that will be subtracted from (subtractFromThis).
 */ 
inline void SubtractTimeVal(struct timeval & subtractFromThis, const struct timeval & subtractThis)
{
   subtractFromThis.tv_sec  -= subtractThis.tv_sec;
   subtractFromThis.tv_usec -= subtractThis.tv_usec;
   if (subtractFromThis.tv_usec < 0L)
   {
      subtractFromThis.tv_sec += (subtractFromThis.tv_usec / MICROS_PER_SECOND)-1;
      while(subtractFromThis.tv_usec < 0L) subtractFromThis.tv_usec += MICROS_PER_SECOND;
   }
}

/** Returns the current real-time clock time as a uint64.  The returned value is expressed
 *  as microseconds since the beginning of the year 1970.
 *  @param timeType if left as MUSCLE_TIMEZONE_UTC (the default), the value returned will be the current UTC time.  
 *                  If set to MUSCLE_TIMEZONE_LOCAL, on the other hand, the returned value will include the proper 
 *                  offset to make the time be the current time as expressed in this machine's local time zone.
 *                  For any other value, the behaviour of this function is undefined.
 *  @note that the values returned by this function are NOT guaranteed to be monotonically increasing!
 *        Events like leap seconds, the user changing the system time, or even the OS tweaking the system
 *        time automatically to eliminate clock drift may cause this value to decrease occasionally!
 *        If you need a time value that is guaranteed to never decrease, you may want to call GetRunTime64() instead.
 */
uint64 GetCurrentTime64(uint32 timeType=MUSCLE_TIMEZONE_UTC);

/** Returns a current time value, in microseconds, that is guaranteed never to decrease.  The absolute
 *  values returned by this call are undefined, so you should only use it for measuring relative time
 *  (i.e. how much time has passed between two events).  For a "wall clock" type of result with
 *  a well-defined time-base, you can call GetCurrentTime64() instead.
 */
#if defined(__BEOS__) || defined(__HAIKU__)
inline uint64 GetRunTime64() {return system_time();}
#elif defined(TARGET_PLATFORM_XENOMAI)
inline uint64 GetRunTime64() {return rt_timer_tsc2ns(rt_timer_tsc())/1000;}
#else
uint64 GetRunTime64();
#endif

/** Given a run-time value, returns the equivalent current-time value.
  * @param runTime64 A run-time value, e.g. as returned by GetRunTime64().
  * @param timeType if left as MUSCLE_TIMEZONE_UTC (the default), the value returned will be the equivalent UTC time.  
  *                 If set to MUSCLE_TIMEZONE_LOCAL, on the other hand, the returned value will include the proper 
  *                 offset to make the time be the equivalent time as expressed in this machine's local time zone.
  *                 For any other value, the behaviour of this function is undefined.
  * @returns a current-time value, as might have been returned by GetCurrentTime64(timeType) at the specified run-time.
  * @note The values returned by this function are only approximate, and may differ slightly from one call to the next.
  *       Of course they will differ significantly if the system's real-time clock value is changed by the user.
  */
inline uint64 GetCurrentTime64ForRunTime64(uint64 runTime64, uint32 timeType=MUSCLE_TIMEZONE_UTC) {return GetCurrentTime64(timeType)+(runTime64-GetRunTime64());}

/** Given a current-time value, returns the equivalent run-time value.
  * @param currentTime64 A current-time value, e.g. as returned by GetCurrentTime64(timeType).
  * @param timeType if left as MUSCLE_TIMEZONE_UTC (the default), the (currentTime64) argument will be interpreted as a UTC time.  
  *                 If set to MUSCLE_TIMEZONE_LOCAL, on the other hand, the (currentTime64) argument will be interpreted as
  *                 a current-time in this machine's local time zone.
  *                 For any other value, the behaviour of this function is undefined.
  * @returns a run-time value, as might have been returned by GetRunTime64() at the specified current-time.
  * @note The values returned by this function are only approximate, and may differ slightly from one call to the next.
  *       Of course they will differ significantly if the system's real-time clock value is changed by the user.
  */
inline uint64 GetRunTime64ForCurrentTime64(uint64 currentTime64, uint32 timeType=MUSCLE_TIMEZONE_UTC) {return GetRunTime64()+(currentTime64-GetCurrentTime64(timeType));}

/** Convenience function:  Won't return for a given number of microsends.
 *  @param micros The number of microseconds to wait for.
 *  @return B_NO_ERROR on success, or B_ERROR on failure.
 */
#if defined(__BEOS__) || defined(__HAIKU__)
inline status_t Snooze64(uint64 microseconds) {return snooze(microseconds);}
#else
status_t Snooze64(uint64 micros);
#endif

/** Convenience function:  Returns true no more often than once every (interval).
 *  Useful if you are in a tight loop, but don't want e.g. more than one debug output line per second, or something like that.
 *  @param interval The minimum time that must elapse between two calls to this function returning true.
 *  @param lastTime A state variable.  Pass in the same reference every time you call this function.
 *  @return true iff it has been at least (interval) since the last time this function returned true, else false.
 */
inline bool OnceEvery(const struct timeval & interval, struct timeval & lastTime)
{
   uint64 now64 = GetRunTime64();
   struct timeval now; 
   Convert64ToTimeVal(now64, now);
   if (!IsLessThan(now, lastTime))
   {
      lastTime = now;
      AddTimeVal(lastTime, interval);
      return true;
   }
   return false;
}

/** Convenience function:  Returns true no more than once every (interval).
 *  Useful if you are in a tight loop, but don't want e.g. more than one debug output line per second, or something like that.
 *  @param interval The minimum time that must elapse between two calls to this function returning true (in microseconds).
 *  @param lastTime A state variable.  Pass in the same reference every time you call this function.  (set to zero the first time)
 *  @return true iff it has been at least (interval) since the last time this function returned true, else false.
 */
inline bool OnceEvery(uint64 interval, uint64 & lastTime)
{
   uint64 now = GetRunTime64();
   if (now >= lastTime+interval)
   {
      lastTime = now;
      return true;
   }
   return false;
}

/** This handy macro will print out, twice a second, 
 *  the average number of times per second it is being called.
 */
#define PRINT_CALLS_PER_SECOND(x) \
{ \
   static uint32 count = 0; \
   static uint64 startTime = 0; \
   uint64 lastTime = 0; \
   uint64 now = GetCurrentTime64(); \
   if (startTime == 0) startTime = now; \
   count++; \
   if ((OnceEvery(500000, lastTime))&&(now>startTime)) printf("%s: " UINT64_FORMAT_SPEC "/s\n", x, (MICROS_PER_SECOND*((uint64)count))/(now-startTime)); \
}

/** @} */ // end of timeutilityfunctions doxygen group

}; // end namespace muscle

#endif
