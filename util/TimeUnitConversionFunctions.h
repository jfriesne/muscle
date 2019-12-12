/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleTimeUnitConversionFunctions_h
#define MuscleTimeUnitConversionFunctions_h

#include <time.h>

#include "support/MuscleSupport.h"

#ifndef WIN32
# include <sys/time.h>
#endif

#if defined(__BEOS__) || defined(__HAIKU__)
# include <kernel/OS.h>
#endif

namespace muscle {

/** @defgroup timeunitconversionfunction The TimeUnitConversionFuncions function API
 *  These functions are all defined in TimeUnitConversionFunctions.h, and are stand-alone
 *  inline functions that provide simple conversion between one unit of time and another.
 *  @{
 */

/** How many milliseconds are in one second (1000) */
#define MILLIS_PER_SECOND  ((int64)1000)

/** How many microseconds are in one second (1000000) */
#define MICROS_PER_SECOND  ((int64)1000000)

/** How many nanoseconds are in one second (1000000000) */
#define NANOS_PER_SECOND   ((int64)1000000000)

/** Given a value in nanoseconds, returns the equivalent number of microseconds. 
  * @param ns A time value, in nanoseconds.
  */
inline MUSCLE_CONSTEXPR int64 NanosToMicros(int64 ns)     {return ns/1000;}

/** Given a value in microseconds, returns the equivalent number of nanoseconds. 
  * @param us A time value, in microseconds.
  */
inline MUSCLE_CONSTEXPR int64 MicrosToNanos(int64 us)     {return us*1000;}

/** Given a value in microseconds, returns the equivalent number of milliseconds. 
  * @param us A time value, in microseconds.
  */
inline MUSCLE_CONSTEXPR int64 MicrosToMillis(int64 us)    {return us/1000;}

/** Given a value in milliseconds, returns the equivalent number of microseconds. 
  * @param ms A time value, in milliseconds.
  */
inline MUSCLE_CONSTEXPR int64 MillisToMicros(int64 ms)    {return ms*1000;}

/** Given a value in milliseconds, returns the equivalent number of nanoseconds. 
  * @param ms A time value, in milliseconds.
  */
inline MUSCLE_CONSTEXPR int64 MillisToSeconds(int64 ms)   {return ms/MILLIS_PER_SECOND;}

/** Given a value in seconds, returns the equivalent number of milliseconds. 
  * @param s A time value, in seconds.
  */
inline MUSCLE_CONSTEXPR int64 SecondsToMillis(int64 s)    {return s*MILLIS_PER_SECOND;}

/** Given a value in seconds, returns the equivalent number of milliseconds. 
  * @param s A time value, in seconds.
  */
inline MUSCLE_CONSTEXPR int64 SecondsToMinutes(int64 s)   {return s/60;}

/** Given a value in minutes, returns the equivalent number of minutes. 
  * @param m A time value, in minutes.
  */
inline MUSCLE_CONSTEXPR int64 MinutesToSeconds(int64 m)   {return m*60;}

/** Given a value in minutes, returns the equivalent number of hours. 
  * @param m A time value, in minutes.
  */
inline MUSCLE_CONSTEXPR int64 MinutesToHours(int64 m)     {return m/60;}

/** Given a value in hours, returns the equivalent number of minutes. 
  * @param h A time value, in hours.
  */
inline MUSCLE_CONSTEXPR int64 HoursToMinutes(int64 h)     {return h*60;}

/** Given a value in hours, returns the equivalent number of days. 
  * @param h A time value, in hours.
  */
inline MUSCLE_CONSTEXPR int64 HoursToDays(int64 h)        {return h/24;}

/** Given a value in days, returns the equivalent number of hours. 
  * @param d A time value, in days.
  */
inline MUSCLE_CONSTEXPR int64 DaysToHours(int64 d)        {return d*24;}

/** Given a value in days, returns the equivalent number of weeks. 
  * @param d A time value, in days.
  */
inline MUSCLE_CONSTEXPR int64 DaysToWeeks(int64 d)        {return d/7;}

/** Given a value in weeks, returns the equivalent number of days. 
  * @param w A time value, in weeks.
  */
inline MUSCLE_CONSTEXPR int64 WeeksToDays(int64 w)        {return w*7;}

/** Given a value in nanoseconds, returns the equivalent number of milliseconds. 
  * @param ns A time value, in nanoseconds.
  */
inline MUSCLE_CONSTEXPR int64 NanosToMillis(int64 ns)     {return MicrosToMillis(NanosToMicros(ns));}

/** Given a value in seconds, returns the equivalent number of microseconds. 
  * @param s A time value, in seconds.
  */
inline MUSCLE_CONSTEXPR int64 SecondsToMicros(int64 s)    {return MillisToMicros(SecondsToMillis(s));}

/** Given a value in microseconds, returns the equivalent number of seconds. 
  * @param us A time value, in microseconds.
  */
inline MUSCLE_CONSTEXPR int64 MicrosToSeconds(int64 us)   {return MillisToSeconds(MicrosToMillis(us));}

/** Given a value in milliseconds, returns the equivalent number of nanoseconds. 
  * @param ms A time value, in milliseconds.
  */
inline MUSCLE_CONSTEXPR int64 MillisToNanos(int64 ms)     {return MicrosToNanos(MillisToMicros(ms));}

/** Given a value in seconds, returns the equivalent number of nanoseconds. 
  * @param s A time value, in seconds.
  */
inline MUSCLE_CONSTEXPR int64 SecondsToNanos(int64 s)     {return MillisToNanos(SecondsToMillis(s));}

/** Given a value in minutes, returns the equivalent number of nanoseconds. 
  * @param m A time value, in minutes.
  */
inline MUSCLE_CONSTEXPR int64 MinutesToNanos(int64 m)     {return SecondsToNanos(MinutesToSeconds(m));}

/** Given a value in minutes, returns the equivalent number of microseconds. 
  * @param m A time value, in minutes.
  */
inline MUSCLE_CONSTEXPR int64 MinutesToMicros(int64 m)    {return SecondsToMicros(MinutesToSeconds(m));}

/** Given a value in minutes, returns the equivalent number of milliseconds. 
  * @param m A time value, in minutes.
  */
inline MUSCLE_CONSTEXPR int64 MinutesToMillis(int64 m)    {return SecondsToMillis(MinutesToSeconds(m));}

/** Given a value in minutes, returns the equivalent number of days. 
  * @param m A time value, in minutes.
  */
inline MUSCLE_CONSTEXPR int64 MinutesToDays(int64 m)      {return HoursToDays(MinutesToHours(m));}

/** Given a value in milliseconds, returns the equivalent number of minutes. 
  * @param ms A time value, in milliseconds.
  */
inline MUSCLE_CONSTEXPR int64 MillisToMinutes(int64 ms)   {return SecondsToMinutes(MillisToSeconds(ms));}

/** Given a value in hours, returns the equivalent number of microseconds. 
  * @param h A time value, in hours.
  */
inline MUSCLE_CONSTEXPR int64 HoursToMicros(int64 h)      {return MinutesToMicros(HoursToMinutes(h));}

/** Given a value in hours, returns the equivalent number of milliseconds. 
  * @param h A time value, in hours.
  */
inline MUSCLE_CONSTEXPR int64 HoursToMillis(int64 h)      {return MinutesToMillis(HoursToMinutes(h));}

/** Given a value in hours, returns the equivalent number of seconds. 
  * @param h A time value, in hours.
  */
inline MUSCLE_CONSTEXPR int64 HoursToSeconds(int64 h)     {return MinutesToSeconds(HoursToMinutes(h));}

/** Given a value in hours, returns the equivalent number of weeks. 
  * @param h A time value, in hours.
  */
inline MUSCLE_CONSTEXPR int64 HoursToWeeks(int64 h)       {return DaysToWeeks(HoursToDays(h));}

/** Given a value in minutes, returns the equivalent number of weeks. 
  * @param m A time value, in minutes.
  */
inline MUSCLE_CONSTEXPR int64 MinutesToWeeks(int64 m)     {return HoursToWeeks(MinutesToHours(m));}

/** Given a value in seconds, returns the equivalent number of hours. 
  * @param s A time value, in seconds.
  */
inline MUSCLE_CONSTEXPR int64 SecondsToHours(int64 s)     {return MinutesToHours(SecondsToMinutes(s));}

/** Given a value in seconds, returns the equivalent number of days. 
  * @param s A time value, in seconds.
  */
inline MUSCLE_CONSTEXPR int64 SecondsToDays(int64 s)      {return MinutesToDays(SecondsToMinutes(s));}

/** Given a value in seconds, returns the equivalent number of weeks. 
  * @param s A time value, in seconds.
  */
inline MUSCLE_CONSTEXPR int64 SecondsToWeeks(int64 s)     {return MinutesToWeeks(SecondsToMinutes(s));}

/** Given a value in milliseconds, returns the equivalent number of hours. 
  * @param ms A time value, in milliseconds.
  */
inline MUSCLE_CONSTEXPR int64 MillisToHours(int64 ms)     {return SecondsToHours(MillisToSeconds(ms));}

/** Given a value in milliseconds, returns the equivalent number of days. 
  * @param ms A time value, in milliseconds.
  */
inline MUSCLE_CONSTEXPR int64 MillisToDays(int64 ms)      {return SecondsToDays(MillisToSeconds(ms));}

/** Given a value in milliseconds, returns the equivalent number of weeks. 
  * @param ms A time value, in milliseconds.
  */
inline MUSCLE_CONSTEXPR int64 MillisToWeeks(int64 ms)     {return SecondsToWeeks(MillisToSeconds(ms));}

/** Given a value in microseconds, returns the equivalent number of minutes. 
  * @param us A time value, in microseconds.
  */
inline MUSCLE_CONSTEXPR int64 MicrosToMinutes(int64 us)   {return MillisToMinutes(MicrosToMillis(us));}

/** Given a value in microseconds, returns the equivalent number of hours. 
  * @param us A time value, in microseconds.
  */
inline MUSCLE_CONSTEXPR int64 MicrosToHours(int64 us)     {return MillisToHours(MicrosToMillis(us));}

/** Given a value in microseconds, returns the equivalent number of days. 
  * @param us A time value, in microseconds.
  */
inline MUSCLE_CONSTEXPR int64 MicrosToDays(int64 us)      {return MillisToDays(MicrosToMillis(us));}

/** Given a value in microseconds, returns the equivalent number of weeks. 
  * @param us A time value, in microseconds.
  */
inline MUSCLE_CONSTEXPR int64 MicrosToWeeks(int64 us)     {return MillisToWeeks(MicrosToMillis(us));}

/** Given a value in nanoseconds, returns the equivalent number of seconds. 
  * @param ns A time value, in nanoseconds.
  */
inline MUSCLE_CONSTEXPR int64 NanosToSeconds(int64 ns)    {return MicrosToSeconds(NanosToMicros(ns));}

/** Given a value in nanoseconds, returns the equivalent number of minutes. 
  * @param ns A time value, in nanoseconds.
  */
inline MUSCLE_CONSTEXPR int64 NanosToMinutes(int64 ns)    {return MicrosToMinutes(NanosToMicros(ns));}

/** Given a value in nanoseconds, returns the equivalent number of hours. 
  * @param ns A time value, in nanoseconds.
  */
inline MUSCLE_CONSTEXPR int64 NanosToHours(int64 ns)      {return MicrosToHours(NanosToMicros(ns));}

/** Given a value in nanoseconds, returns the equivalent number of days. 
  * @param ns A time value, in nanoseconds.
  */
inline MUSCLE_CONSTEXPR int64 NanosToDays(int64 ns)       {return MicrosToDays(NanosToMicros(ns));}

/** Given a value in nanoseconds, returns the equivalent number of weeks. 
  * @param ns A time value, in nanoseconds.
  */
inline MUSCLE_CONSTEXPR int64 NanosToWeeks(int64 ns)      {return MicrosToWeeks(NanosToMicros(ns));}

/** Given a value in hours, returns the equivalent number of nanoseconds. 
  * @param h A time value, in hours.
  */
inline MUSCLE_CONSTEXPR int64 HoursToNanos(int64 h)       {return MinutesToNanos(HoursToMinutes(h));}

/** Given a value in days, returns the equivalent number of nanoseconds. 
  * @param d A time value, in days.
  */
inline MUSCLE_CONSTEXPR int64 DaysToNanos(int64 d)        {return HoursToNanos(DaysToHours(d));}

/** Given a value in days, returns the equivalent number of microseconds. 
  * @param d A time value, in days.
  */
inline MUSCLE_CONSTEXPR int64 DaysToMicros(int64 d)       {return HoursToMicros(DaysToHours(d));}

/** Given a value in days, returns the equivalent number of milliseconds. 
  * @param d A time value, in days.
  */
inline MUSCLE_CONSTEXPR int64 DaysToMillis(int64 d)       {return HoursToMillis( DaysToHours(d));}

/** Given a value in days, returns the equivalent number of seconds. 
  * @param d A time value, in days.
  */
inline MUSCLE_CONSTEXPR int64 DaysToSeconds(int64 d)      {return HoursToSeconds(DaysToHours(d));}

/** Given a value in days, returns the equivalent number of minutes. 
  * @param d A time value, in days.
  */
inline MUSCLE_CONSTEXPR int64 DaysToMinutes(int64 d)      {return HoursToMinutes(DaysToHours(d));}

/** Given a value in weeks, returns the equivalent number of nanoseconds. 
  * @param w A time value, in weeks.
  */
inline MUSCLE_CONSTEXPR int64 WeeksToNanos(int64 w)       {return DaysToNanos(WeeksToDays(w));}

/** Given a value in weeks, returns the equivalent number of microseconds. 
  * @param w A time value, in weeks.
  */
inline MUSCLE_CONSTEXPR int64 WeeksToMicros(int64 w)      {return DaysToMicros(WeeksToDays(w));}

/** Given a value in weeks, returns the equivalent number of milliseconds. 
  * @param w A time value, in weeks.
  */
inline MUSCLE_CONSTEXPR int64 WeeksToMillis(int64 w)      {return DaysToMillis(WeeksToDays(w));}

/** Given a value in weeks, returns the equivalent number of seconds. 
  * @param w A time value, in weeks.
  */
inline MUSCLE_CONSTEXPR int64 WeeksToSeconds(int64 w)     {return DaysToSeconds(WeeksToDays(w));}

/** Given a value in weeks, returns the equivalent number of minutes. 
  * @param w A time value, in weeks.
  */
inline MUSCLE_CONSTEXPR int64 WeeksToMinutes(int64 w)     {return DaysToMinutes(WeeksToDays(w));}

/** Given a value in weeks, returns the equivalent number of hours. 
  * @param w A time value, in weeks.
  */
inline MUSCLE_CONSTEXPR int64 WeeksToHours(int64 w)       {return DaysToHours(WeeksToDays(w));}

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
 *  @returns true iff t1 < t2
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

/** @} */ // end of timeunitconversionfunctions doxygen group

} // end namespace muscle

#endif
