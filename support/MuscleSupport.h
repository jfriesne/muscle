/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

/******************************************************************************
/
/     File:     MuscleSupport.h
/
/     Description:  Standard types, macros, functions, etc, for MUSCLE.
/
*******************************************************************************/

#ifndef MuscleSupport_h
#define MuscleSupport_h

#define MUSCLE_VERSION_STRING "6.11"
#define MUSCLE_VERSION        61100  // Format is decimal Mmmbb, where (M) is the number before the decimal point, (mm) is the number after the decimal point, and (bb) is reserved

/*! \mainpage MUSCLE Documentation Page
 *
 * The MUSCLE API provides a robust, somewhat scalable, cross-platform client-server solution for 
 * network-distributed applications for Linux, MacOS/X, BSD, Windows, BeOS, AtheOS, and other operating 
 * systems.  It allows (n) client programs (each of which may be running on a separate computer and/or 
 * under a different OS) to communicate with each other in a many-to-many message-passing style.  It 
 * employs a central server to which client programs may connect or disconnect at any time  (This design 
 * is similar to other client-server systems such as Quake servers, IRC servers, and Napster servers, 
 * but more general in application).  In addition to the client-server system, MUSCLE contains classes 
 * to support peer-to-peer message streaming connections, as well as some handy miscellaneous utility 
 * classes, all of which are documented here.
 *
 * All classes documented here should compile under most modern OS's with a modern C++ compiler.
 * Where platform-specific code is necessary, it has been provided (inside \#ifdef's) for various OS's.
 * Templates are used throughout; exceptions are not.  The code is usable in multithreaded environments,
 * as long as you are careful.
 *
 * As distributed, the server side of the software is ready to compile and run, but to do much with it 
 * you'll want to write your own client software.  Example client software can be found in the "test" 
 * subdirectory.
 */

#include <assert.h>  /* for assert() */
#include <string.h>  /* for memcpy() */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>   /* for errno */

/* Define this if the default FD_SETSIZE is too small for you (i.e. under Windows it's only 64) */
#if defined(MUSCLE_FD_SETSIZE)
# if defined(FD_SETSIZE)
#  error "MuscleSupport.h:  Can not redefine FD_SETSIZE, someone else has already defined it!  You need to include MuscleSupport.h before including any other header files that define FD_SETSIZE."
# else
#  define FD_SETSIZE MUSCLE_FD_SETSIZE
# endif
#endif

/* If we are in an environment where known assembly is available, make a note of that fact */
#ifndef MUSCLE_AVOID_INLINE_ASSEMBLY
# if defined(__GNUC__)
#  if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(__amd64__) || defined(__x86_64__) || defined(__x86__) || defined(__pentium__) || defined(__pentiumpro__) || defined(__k6__) || defined(_M_AMD64)
#   define MUSCLE_USE_X86_INLINE_ASSEMBLY 1
#  elif defined(__PPC__) || defined(__POWERPC__)
#   define MUSCLE_USE_POWERPC_INLINE_ASSEMBLY 1
#  endif
# elif defined(_MSC_VER) && (defined(_X86_) || defined(_M_IX86))
#  define MUSCLE_USE_X86_INLINE_ASSEMBLY 1
# endif
#endif

#if (_MSC_VER >= 1310)
# define MUSCLE_USE_MSVC_SWAP_FUNCTIONS 1
#endif

#if (_MSC_VER >= 1300) && !defined(MUSCLE_AVOID_WINDOWS_STACKTRACE)
# define MUSCLE_USE_MSVC_STACKWALKER 1
#endif


#ifdef __cplusplus
# ifdef MUSCLE_USE_CPLUSPLUS11
#  include <type_traits>  // for static_assert()
#  include <utility>      // for std::move()
# endif
#else
# define NEW_H_NOT_AVAILABLE
#endif

/* Borland C++ builder also runs under Win32, but it doesn't set this flag So we'd better set it ourselves. */
#if defined(__BORLANDC__) || defined(__WIN32__) || defined(_MSC_VER)
# ifndef WIN32
#  define WIN32 1
# endif
#endif

/* Win32 can't handle this stuff, it's too lame */
#ifdef WIN32
# define SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
# define UNISTD_H_NOT_AVAILABLE
# ifndef _MSC_VER  /* 7/3/2006: Mika's patch allows VC++ to use newnothrow */
#  define NEW_H_NOT_AVAILABLE
# endif
#elif defined(__BEOS__) && !defined(__HAIKU__)
# define SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
#endif

#ifndef UNISTD_H_NOT_AVAILABLE
# include <unistd.h>
#endif

#ifndef NEW_H_NOT_AVAILABLE
# include <new>
# ifndef __MWERKS__
using std::bad_alloc;
using std::nothrow_t;
using std::nothrow;
#  if (defined(_MSC_VER))
// VC++ 6.0 and earlier lack this definition
#   if (_MSC_VER < 1300)
inline void __cdecl operator delete(void *p, const std::nothrow_t&) _THROW0() {delete(p);}
#   endif
#  else
using std::new_handler;
using std::set_new_handler;
#  endif
# endif
#else
# define MUSCLE_AVOID_NEWNOTHROW
#endif

#ifndef newnothrow
# ifdef MUSCLE_AVOID_NEWNOTHROW
#  define newnothrow new
# else
#  define newnothrow new (nothrow)
# endif
#endif

// These macros are implementation details, please ignore them
#define ___MUSCLE_UNIQUE_NAME_AUX1(name, line) name##line
#define ___MUSCLE_UNIQUE_NAME_AUX2(name, line) ___MUSCLE_UNIQUE_NAME_AUX1(name, line)

/** This macro expands to a variable name which is (per-file) unique and unreferenceable.
  * This can be useful to help make sure the temporary variables in your macros don't
  * collide with each other.
  */
#define MUSCLE_UNIQUE_NAME ___MUSCLE_UNIQUE_NAME_AUX2(__uniqueName, __LINE__)

/** This macro declares an object on the stack with the specified argument. */
#ifdef _MSC_VER
# define DECLARE_ANONYMOUS_STACK_OBJECT(objType, ...) objType MUSCLE_UNIQUE_NAME = objType(__VA_ARGS__)
#else
# define DECLARE_ANONYMOUS_STACK_OBJECT(objType, args...) objType MUSCLE_UNIQUE_NAME = objType(args)
#endif

#ifdef MUSCLE_AVOID_ASSERTIONS
# define MASSERT(x,msg)
#else
# define MASSERT(x,msg) {if(!(x)) MCRASH(msg)}
#endif

#ifdef WIN32
# define MCRASH(msg) {muscle::LogTime(muscle::MUSCLE_LOG_CRITICALERROR, "ASSERTION FAILED: (%s:%i) %s\n", __FILE__,__LINE__,msg); muscle::LogStackTrace(MUSCLE_LOG_CRITICALERROR); RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);}
#else
# define MCRASH(msg) {muscle::LogTime(muscle::MUSCLE_LOG_CRITICALERROR, "ASSERTION FAILED: (%s:%i) %s\n", __FILE__,__LINE__,msg); muscle::LogStackTrace(MUSCLE_LOG_CRITICALERROR); abort();}
#endif

#define MEXIT(retVal,msg) {muscle::LogTime(muscle::MUSCLE_LOG_CRITICALERROR, "ASSERTION FAILED: (%s:%i) %s\n", __FILE__,__LINE__,msg); muscle::LogStackTrace(MUSCLE_LOG_CRITICALERROR); ExitWithoutCleanup(retVal);}
#define WARN_OUT_OF_MEMORY muscle::WarnOutOfMemory(__FILE__, __LINE__)
#define MCHECKPOINT muscle::LogTime(muscle::MUSCLE_LOG_WARNING, "Reached checkpoint at %s:%i\n", __FILE__, __LINE__)

#ifdef __cplusplus
template<typename T, int size> unsigned int ARRAYITEMS(T(&)[size]) {return size;}  /* returns # of items in array, will error out at compile time if you try it on a pointer */
#else
# define ARRAYITEMS(x) (sizeof(x)/sizeof(x[0]))  /* returns # of items in array */
#endif
typedef void * muscleVoidPointer;  /* it's a bit easier, syntax-wise, to use this type than (void *) directly in some cases. */

#if defined(__BEOS__) || defined(__HAIKU__)
# include <support/Errors.h>
# include <support/ByteOrder.h>  /* might as well use the real thing (and avoid complaints about duplication) */
# include <support/SupportDefs.h>
# include <support/TypeConstants.h>
# if !((defined(BONE))||(defined(BONE_VERSION))||(defined(__HAIKU__)))
#  define BEOS_OLD_NETSERVER
# endif
#else
# define B_ERROR    -1
# define B_NO_ERROR 0
# define B_OK       B_NO_ERROR
# ifdef __ATHEOS__
#  include </ainc/atheos/types.h>
# else
#  ifndef MUSCLE_TYPES_PREDEFINED  /* certain (ahem) projects already set these themselves... */
#   ifndef __cplusplus
#    define true                     1
#    define false                    0
#   endif
    typedef unsigned char           uchar;
    typedef unsigned short          unichar;
    typedef signed char             int8;
    typedef unsigned char           uint8;
    typedef short                   int16;
    typedef unsigned short          uint16;
#   if defined(MUSCLE_64_BIT_PLATFORM) || defined(__osf__) || defined(__amd64__) || defined(__PPC64__) || defined(__x86_64__) || defined(_M_AMD64)   /* some 64bit systems will have long=64-bit, int=32-bit */
#    ifndef MUSCLE_64_BIT_PLATFORM
#     define MUSCLE_64_BIT_PLATFORM 1  // auto-define it if it wasn't defined in the Makefile
#    endif
     typedef int                    int32;
#    ifndef _UINT32   // Avoid conflict with typedef in OS/X Leopard system header
      typedef unsigned int           uint32;
#     define _UINT32  // Avoid conflict with typedef in OS/X Leopard system header
#    endif
#   else
     typedef long                   int32;
#    ifndef _UINT32   // Avoid conflict with typedef in OS/X Leopard system header
      typedef unsigned long          uint32;
#     define _UINT32  // Avoid conflict with typedef in OS/X Leopard system header
#    endif
#   endif
#   if defined(WIN32) && !defined(__GNUWIN32__)
     typedef __int64                int64;
     typedef unsigned __int64       uint64;
#   elif __APPLE__
#    ifndef _UINT64  // Avoid conflict with typedef in OS/X Leopard system header
#     define _UINT64
      typedef long long              int64;   // these are what's expected in MacOS/X land, in both
      typedef unsigned long long     uint64;  // 32-bit and 64-bit flavors.  C'est la vie, non?
#    endif
#   elif defined(MUSCLE_64_BIT_PLATFORM)
     typedef long                   int64;
     typedef unsigned long          uint64;
#   else
     typedef long long              int64;
     typedef unsigned long long     uint64;
#   endif
    typedef int32                     status_t;
#  endif  /* !MUSCLE_TYPES_PREDEFINED */
# endif  /* !__ATHEOS__*/
#endif  /* __BEOS__ || __HAIKU__ */

/** Ugly platform-neutral macros for problematic sprintf()-format-strings */
#if defined(MUSCLE_64_BIT_PLATFORM)
# define  INT32_FORMAT_SPEC_NOPERCENT "i"
# define UINT32_FORMAT_SPEC_NOPERCENT "u"
# define XINT32_FORMAT_SPEC_NOPERCENT "x"
# ifdef __APPLE__
#  define  INT64_FORMAT_SPEC_NOPERCENT "lli"
#  define UINT64_FORMAT_SPEC_NOPERCENT "llu"
#  define XINT64_FORMAT_SPEC_NOPERCENT "llx"
# else
#  define  INT64_FORMAT_SPEC_NOPERCENT "li"
#  define UINT64_FORMAT_SPEC_NOPERCENT "lu"
#  define XINT64_FORMAT_SPEC_NOPERCENT "lx"
# endif
#else
# define  INT32_FORMAT_SPEC_NOPERCENT "li"
# define UINT32_FORMAT_SPEC_NOPERCENT "lu"
# define XINT32_FORMAT_SPEC_NOPERCENT "lx"
# if defined(_MSC_VER)
#  define  INT64_FORMAT_SPEC_NOPERCENT "I64i"
#  define UINT64_FORMAT_SPEC_NOPERCENT "I64u"
#  define XINT64_FORMAT_SPEC_NOPERCENT "I64x"
# elif (defined(__BEOS__) && !defined(__HAIKU__)) || defined(__MWERKS__) || defined(__BORLANDC__)
#  define  INT64_FORMAT_SPEC_NOPERCENT "Li"
#  define UINT64_FORMAT_SPEC_NOPERCENT "Lu"
#  define XINT64_FORMAT_SPEC_NOPERCENT "Lx"
# else
#  define  INT64_FORMAT_SPEC_NOPERCENT "lli"
#  define UINT64_FORMAT_SPEC_NOPERCENT "llu"
#  define XINT64_FORMAT_SPEC_NOPERCENT "llx"
# endif
#endif

# define  INT32_FORMAT_SPEC "%"  INT32_FORMAT_SPEC_NOPERCENT
# define UINT32_FORMAT_SPEC "%" UINT32_FORMAT_SPEC_NOPERCENT
# define XINT32_FORMAT_SPEC "%" XINT32_FORMAT_SPEC_NOPERCENT

# define  INT64_FORMAT_SPEC "%"  INT64_FORMAT_SPEC_NOPERCENT
# define UINT64_FORMAT_SPEC "%" UINT64_FORMAT_SPEC_NOPERCENT
# define XINT64_FORMAT_SPEC "%" XINT64_FORMAT_SPEC_NOPERCENT

#define MAKETYPE(x) ((((unsigned long)(x[0])) << 24) | \
                     (((unsigned long)(x[1])) << 16) | \
                     (((unsigned long)(x[2])) <<  8) | \
                     (((unsigned long)(x[3])) <<  0))

#if !defined(__BEOS__) && !defined(__HAIKU__)
/* Be-style message-field type codes.
 * I've calculated the integer equivalents for these codes
 * because gcc whines like a little girl about the four-byte
 * constants when compiling under Linux --jaf
 */
enum {
   B_ANY_TYPE     = 1095653716, /* 'ANYT' = wild card                                   */
   B_BOOL_TYPE    = 1112493900, /* 'BOOL' = boolean (1 byte per bool)                   */
   B_DOUBLE_TYPE  = 1145195589, /* 'DBLE' = double-precision float (8 bytes per double) */
   B_FLOAT_TYPE   = 1179406164, /* 'FLOT' = single-precision float (4 bytes per float)  */
   B_INT64_TYPE   = 1280069191, /* 'LLNG' = long long integer (8 bytes per int)         */
   B_INT32_TYPE   = 1280265799, /* 'LONG' = long integer (4 bytes per int)              */
   B_INT16_TYPE   = 1397248596, /* 'SHRT' = short integer (2 bytes per int)             */
   B_INT8_TYPE    = 1113150533, /* 'BYTE' = byte integer (1 byte per int)               */
   B_MESSAGE_TYPE = 1297303367, /* 'MSGG' = sub Message objects (reference counted)     */
   B_POINTER_TYPE = 1347310674, /* 'PNTR' = pointers (will not be flattened)            */
   B_POINT_TYPE   = 1112559188, /* 'BPNT' = Point objects (each Point has two floats)   */
   B_RECT_TYPE    = 1380270932, /* 'RECT' = Rect objects (each Rect has four floats)    */
   B_STRING_TYPE  = 1129534546, /* 'CSTR' = String objects (variable length)            */
   B_OBJECT_TYPE  = 1330664530, /* 'OPTR' = Flattened user objects (obsolete)           */
   B_RAW_TYPE     = 1380013908, /* 'RAWT' = Raw data (variable number of bytes)         */
   B_MIME_TYPE    = 1296649541  /* 'MIME' = MIME strings (obsolete)                     */
};
#endif

/* This one isn't defined by BeOS, so we have to enumerate it separately.               */
enum {
   B_TAG_TYPE     = 1297367367  /* 'MTAG' = new for v2.00; for in-mem-only tags         */
};

/* This constant is used in various places to mean 'as much as you want' */
#define MUSCLE_NO_LIMIT ((uint32)-1)

#ifdef __cplusplus

/** A handy little method to swap the bytes of any int-style datatype around */
template<typename T> inline T muscleSwapBytes(T swapMe)
{
   union {T _iWide; uint8 _i8[sizeof(T)];} u1, u2;
   u1._iWide = swapMe;

   int i = 0;
   int numBytes = sizeof(T);
   while(numBytes>0) u2._i8[i++] = u1._i8[--numBytes];
   return u2._iWide;
}

/* This template safely copies a value in from an untyped byte buffer to a typed value, and returns the typed value.  */
template<typename T> inline T muscleCopyIn(const void * source) {T dest; memcpy(&dest, source, sizeof(dest)); return dest;}

/* This template safely copies a value in from an untyped byte buffer to a typed value.  */
template<typename T> inline void muscleCopyIn(T & dest, const void * source) {memcpy(&dest, source, sizeof(dest));}

/** This template safely copies a value in from a typed value to an untyped byte buffer.  */
template<typename T> inline void muscleCopyOut(void * dest, const T & source) {memcpy(dest, &source, sizeof(source));}

/** This macro should be used instead of "newnothrow T[count]".  It works the
  * same, except that it hacks around an ugly bug in gcc 3.x where newnothrow
  * would return ((T*)0x4) on memory failure instead of NULL.
  * See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=10300
  */
#if __GNUC__ == 3
template <typename T> inline T * broken_gcc_newnothrow_array(size_t count)
{
   T * ret = newnothrow T[count];
   return (ret <= (T *)(sizeof(void *))) ? NULL : ret;
}
# define newnothrow_array(T, count) broken_gcc_newnothrow_array<T>(count)
#else
# define newnothrow_array(T, count) newnothrow T[count]
#endif

/** This function returns a reference to a read-only, default-constructed
  * static object of type T.  There will be exactly one of these objects
  * present per instantiated type, per process.
  */
template <typename T> const T & GetDefaultObjectForType()
{
   static T _defaultObject;
   return _defaultObject;
}

/** This function returns a reference to a read/write, default-constructed
  * static object of type T.  There will be exactly one of these objects
  * present per instantiated type, per process.
  *
  * Note that this object is different from the one returned by
  * GetDefaultObjectForType(); the difference (other than there being
  * possibly two separate objects) is that this one can be written to,
  * whereas GetDefaultObjectForType()'s object must always be in its default state.
  */
template <typename T> T & GetGlobalObjectForType()
{
   static T _defaultObject;
   return _defaultObject;
}

#if __GNUC__ >= 4
# define MUSCLE_MAY_ALIAS __attribute__((__may_alias__))
#else
# define MUSCLE_MAY_ALIAS
#endif

#ifdef MUSCLE_USE_CPLUSPLUS11

/** Returns the smaller of the two arguments */
template<typename T> T muscleMin(T arg1, T arg2) {return (arg1<arg2) ? arg1 : arg2;}

/** Returns the smallest of all of the arguments */
template<typename T1, typename ...T2> T1 muscleMin(T1 arg1, T2... args) {return muscleMin(arg1, muscleMin(args...));}

/** Returns the larger of the two arguments */
template<typename T> T muscleMax(T arg1, T arg2) {return (arg1>arg2) ? arg1 : arg2;}

/** Returns the largest of all of the arguments */
template<typename T1, typename ...T2> T1 muscleMax(T1 arg1, T2... args) {return muscleMax(arg1, muscleMax(args...));}

#else

/** Returns the smaller of the two arguments */
template<typename T> inline T muscleMin(T p1, T p2) {return (p1 < p2) ? p1 : p2;}

/** Returns the smallest of the three arguments */
template<typename T> inline T muscleMin(T p1, T p2, T p3) {return muscleMin(p3, muscleMin(p1, p2));}

/** Returns the smallest of the four arguments */
template<typename T> inline T muscleMin(T p1, T p2, T p3, T p4) {return muscleMin(p3, p4, muscleMin(p1, p2));}

/** Returns the smallest of the five arguments */
template<typename T> inline T muscleMin(T p1, T p2, T p3, T p4, T p5) {return muscleMin(p3, p4, p5, muscleMin(p1, p2));}

/** Returns the larger of the two arguments */
template<typename T> inline T muscleMax(T p1, T p2) {return (p1 < p2) ? p2 : p1;}

/** Returns the largest of the three arguments */
template<typename T> inline T muscleMax(T p1, T p2, T p3) {return muscleMax(p3, muscleMax(p1, p2));}

/** Returns the largest of the four arguments */
template<typename T> inline T muscleMax(T p1, T p2, T p3, T p4) {return muscleMax(p3, p4, muscleMax(p1, p2));}

/** Returns the largest of the five arguments */
template<typename T> inline T muscleMax(T p1, T p2, T p3, T p4, T p5) {return muscleMax(p3, p4, p5, muscleMax(p1, p2));}

#endif

#if defined(__GNUC__) && (__GNUC__ <= 3) && !defined(MUSCLE_AVOID_AUTOCHOOSE_SWAP)
# define MUSCLE_AVOID_AUTOCHOOSE_SWAP
#endif

#ifdef MUSCLE_AVOID_AUTOCHOOSE_SWAP

/** Swaps the two arguments.  (Primitive version, used only for old compilers like G++ 3.x that can't handle the SFINAE)
  * @param t1 First item to swap.  After this method returns, it will be equal to the old value of t2.
  * @param t2 Second item to swap.  After this method returns, it will be equal to the old value of t1.
  */
template<typename T> inline void muscleSwap(T & t1, T & t2) {T t=t1; t1 = t2; t2 = t;}

#else

namespace ugly_swapcontents_method_sfinae_implementation
{
   // This code was from the example code at http://www.martinecker.com/wiki/index.php?title=Detecting_the_Existence_of_Member_Functions_at_Compile-Time
   // It is used by the muscleSwap() function (below) to automatically call SwapContents() if such a method
   // is available, otherwise muscleSwap() will use a naive copy-to-temporary-object technique.

   typedef char yes;
   typedef char (&no)[2];
   template <typename T, void (T::*f)(T&)> struct test_swapcontents_wrapper {};

   // via SFINAE only one of these overloads will be considered
   template <typename T> yes swapcontents_tester(test_swapcontents_wrapper<T, &T::SwapContents>*);
   template <typename T> no  swapcontents_tester(...);

   template <typename T> struct test_swapcontents_impl {static const bool value = sizeof(swapcontents_tester<T>(0)) == sizeof(yes);};

   template <class T> struct has_swapcontents_method : test_swapcontents_impl<T> {};
   template <bool Condition, typename TrueResult, typename FalseResult> struct if_;
   template <typename TrueResult, typename FalseResult> struct if_<true,  TrueResult, FalseResult> {typedef TrueResult  result;};
   template <typename TrueResult, typename FalseResult> struct if_<false, TrueResult, FalseResult> {typedef FalseResult result;};

   template<typename T> class PODSwapper
   {
   public:
      PODSwapper(T & t1, T & t2)
      {
#ifdef MUSCLE_USE_CPLUSPLUS11
         T tmp(std::move(t1));
         t1 = std::move(t2); 
         t2 = std::move(tmp);
#else
         T tmp = t1;
         t1 = t2;
         t2 = tmp;
#endif
      }
   };

   template<typename T> class SwapContentsSwapper
   {
   public:
      SwapContentsSwapper(T & t1, T & t2) {t1.SwapContents(t2);}
   };

   template<typename ItemType> class AutoChooseSwapperHelper
   {
   public:
      typedef typename if_<test_swapcontents_impl<ItemType>::value, SwapContentsSwapper<ItemType>, PODSwapper<ItemType> >::result Type;
   };
}

/** Swaps the two arguments.
  * @param t1 First item to swap.  After this method returns, it will be equal to the old value of t2.
  * @param t2 Second item to swap.  After this method returns, it will be equal to the old value of t1.
  */
template<typename T> inline void muscleSwap(T & t1, T & t2) {typename ugly_swapcontents_method_sfinae_implementation::AutoChooseSwapperHelper<T>::Type swapper(t1,t2);}

#endif

/** Returns true iff (i) is a valid index into array (a)
  * @param i An index value
  * @param array an array of any type
  * @returns True iff i is non-negative AND less than ARRAYITEMS(array))
  */
template<typename T, int size> inline bool muscleArrayIndexIsValid(int i, T(&)[size] /*array*/) {return (((unsigned int)i) < size);}

/** Returns the value nearest to (v) that is still in the range [lo, hi].
  * @param v A value
  * @param lo a minimum value
  * @param hi a maximum value
  * @returns The value in the range [lo, hi] that is closest to (v).
  */
template<typename T> inline T muscleClamp(T v, T lo, T hi) {return (v < lo) ? lo : ((v > hi) ? hi : v);}

/** Returns true iff (v) is in the range [lo,hi].
  * @param v A value
  * @param lo A minimum value
  * @param hi A maximum value
  * @returns true iff (v >= lo) and (v <= hi)
  */
template<typename T> inline bool muscleInRange(T v, T lo, T hi) {return ((v >= lo)&&(v <= hi));}

/** Returns -1 if arg1 is larger, or 1 if arg2 is larger, or 0 if they are equal.
  * @param arg1 First item to compare
  * @param arg2 Second item to compare
  * @returns -1 if (arg1<arg2), or 1 if (arg2<arg1), or 0 otherwise.
  */
template<typename T> inline int muscleCompare(const T & arg1, const T & arg2) {return (arg2<arg1) ? 1 : ((arg1<arg2) ? -1 : 0);}

/** Returns the absolute value of (arg) */
template<typename T> inline T muscleAbs(T arg) {return (arg<0)?(-arg):arg;}

/** Rounds the given float to the nearest integer value. */
inline int muscleRintf(float f) {return (f>=0.0f) ? ((int)(f+0.5f)) : -((int)((-f)+0.5f));}

/** Returns -1 if the value is less than zero, +1 if it is greater than zero, or 0 otherwise. */
template<typename T> inline int muscleSgn(T arg) {return (arg<0)?-1:((arg>0)?1:0);}

#endif  /* __cplusplus */

#if !defined(__BEOS__) && !defined(__HAIKU__)

/*
 * Copyright(c) 1983,   1989
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *      from nameser.h  8.1 (Berkeley) 6/2/93
 */

#ifndef BYTE_ORDER
  #if (BSD >= 199103)
    #include <machine/endian.h>
  #else
    #if defined(linux) || defined(__linux__)
      #include <endian.h>
    #elif defined( __APPLE__ )
      #include <machine/endian.h>
    #else
      #define LITTLE_ENDIAN   1234    /* least-significant byte first (vax, pc) */
      #define BIG_ENDIAN      4321    /* most-significant byte first (IBM, net) */

      #if defined(vax) || defined(ns32000) || defined(sun386) || defined(i386) || \
              defined(__i386) || defined(__ia64) || \
              defined(MIPSEL) || defined(_MIPSEL) || defined(BIT_ZERO_ON_RIGHT) || \
              defined(__alpha__) || defined(__alpha) || defined(__CYGWIN__) || \
              defined(_M_IX86) || defined(_M_AMD64) || defined(__GNUWIN32__) || defined(__LITTLEENDIAN__) || \
              (defined(__Lynx__) && defined(__x86__))
        #define BYTE_ORDER      LITTLE_ENDIAN
      #endif

      #if defined(__POWERPC__) || defined(sel) || defined(pyr) || defined(mc68000) || defined(sparc) || \
          defined(__sparc) || \
          defined(is68k) || defined(tahoe) || defined(ibm032) || defined(ibm370) || \
          defined(MIPSEB) || defined(_MIPSEB) || defined(_IBMR2) || defined(DGUX) ||\
          defined(apollo) || defined(__convex__) || defined(_CRAY) || \
          defined(__hppa) || defined(__hp9000) || \
          defined(__hp9000s300) || defined(__hp9000s700) || \
          defined(__hp3000s900) || defined(MPE) || \
          defined(BIT_ZERO_ON_LEFT) || defined(m68k) || \
              (defined(__Lynx__) && \
              (defined(__68k__) || defined(__sparc__) || defined(__powerpc__)))
        #define BYTE_ORDER      BIG_ENDIAN
      #endif
    #endif /* linux */
  #endif /* BSD */
#endif /* BYTE_ORDER */

#if !defined(BYTE_ORDER) || (BYTE_ORDER != BIG_ENDIAN && BYTE_ORDER != LITTLE_ENDIAN)
        /*
         * you must determine what the correct bit order is for
         * your compiler - the next line is an intentional error
         * which will force your compiles to bomb until you fix
         * the above macros.
         */
#       error "Undefined or invalid BYTE_ORDER -- you will need to modify MuscleSupport.h to correct this";
#endif

/* End replacement code from Sun/University of California */

/***********************************************************************************************
 * FLOAT_TROUBLE COMMENT
 *
 * NOTE: The *_FLOAT_* macros listed below are obsolete and must no longer be used, because
 * they are inherently unsafe.  The reason is that on certain processors (read x86), the
 * byte-swapped representation of certain floating point and double values can end up
 * representing an invalid value (NaN)... in which case the x86 FPU feels free to munge
 * some of the bits into a "standard NaN" bit-pattern... causing silent data corruption
 * when the value is later swapped back into its native form and again interpreted as a
 * float or double value.  Instead, you need to change your code to use the *_IFLOAT_*
 * macros, which work similarly except that the externalized value is safely stored as
 * a int32 (for floats) or a int64 (for doubles).  --Jeremy 1/8/2007
 **********************************************************************************************/

#define B_HOST_TO_BENDIAN_FLOAT(arg) B_HOST_TO_BENDIAN_FLOAT_was_removed_use_B_HOST_TO_BENDIAN_IFLOAT_instead___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
#define B_HOST_TO_LENDIAN_FLOAT(arg) B_HOST_TO_LENDIAN_FLOAT_was_removed_use_B_HOST_TO_LENDIAN_IFLOAT_instead___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
#define B_BENDIAN_TO_HOST_FLOAT(arg) B_BENDIAN_TO_HOST_FLOAT_was_removed_use_B_BENDIAN_TO_HOST_IFLOAT_instead___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
#define B_LENDIAN_TO_HOST_FLOAT(arg) B_LENDIAN_TO_HOST_FLOAT_was_removed_use_B_LENDIAN_TO_HOST_IFLOAT_instead___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
#define B_SWAP_FLOAT(arg)            B_SWAP_FLOAT_was_removed___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details.

/***********************************************************************************************
 * DOUBLE_TROUBLE COMMENT
 *
 * NOTE: The *_DOUBLE_* macros listed below are obsolete and must no longer be used, because
 * they are inherently unsafe.  The reason is that on certain processors (read x86), the
 * byte-swapped representation of certain floating point and double values can end up
 * representing an invalid value (NaN)... in which case the x86 FPU feels free to munge
 * some of the bits into a "standard NaN" bit-pattern... causing silent data corruption
 * when the value is later swapped back into its native form and again interpreted as a
 * float or double value.  Instead, you need to change your code to use the *_IDOUBLE_*
 * macros, which work similarly except that the externalized value is safely stored as
 * a int32 (for floats) or a int64 (for doubles).  --Jeremy 1/8/2007
 **********************************************************************************************/

#define B_HOST_TO_BENDIAN_DOUBLE(arg) B_HOST_TO_BENDIAN_DOUBLE_was_removed_use_B_HOST_TO_BENDIAN_IDOUBLE_instead___See_the_DOUBLE_TROUBLE_comment_in_support/MuscleSupport_h_for_details_
#define B_HOST_TO_LENDIAN_DOUBLE(arg) B_HOST_TO_LENDIAN_DOUBLE_was_removed_use_B_HOST_TO_LENDIAN_IDOUBLE_instead___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
#define B_BENDIAN_TO_HOST_DOUBLE(arg) B_BENDIAN_TO_HOST_DOUBLE_was_removed_use_B_BENDIAN_TO_HOST_IDOUBLE_instead___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
#define B_LENDIAN_TO_HOST_DOUBLE(arg) B_LENDIAN_TO_HOST_DOUBLE_was_removed_use_B_LENDIAN_TO_HOST_IDOUBLE_instead___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
#define B_SWAP_DOUBLE(arg)            B_SWAP_DOUBLE_was_removed___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_

# if defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY)
static inline uint16 MusclePowerPCSwapInt16(uint16 val)
{
   uint16 a;
   uint16 * addr = &a;
   __asm__ ("sthbrx %1,0,%2" : "=m" (*addr) : "r" (val), "r" (addr));
   return a;
}
static inline uint32 MusclePowerPCSwapInt32(uint32 val)
{
   uint32 a;
   uint32 * addr = &a;
   __asm__ ("stwbrx %1,0,%2" : "=m" (*addr) : "r" (val), "r" (addr));
   return a;
}
static inline uint64 MusclePowerPCSwapInt64(uint64 val)
{
   return ((uint64)(MusclePowerPCSwapInt32((uint32)((val>>32)&0xFFFFFFFF))))|(((uint64)(MusclePowerPCSwapInt32((uint32)(val&0xFFFFFFFF))))<<32);
}
#  define B_SWAP_INT64(arg)    MusclePowerPCSwapInt64((uint64)(arg))
#  define B_SWAP_INT32(arg)    MusclePowerPCSwapInt32((uint32)(arg))
#  define B_SWAP_INT16(arg)    MusclePowerPCSwapInt16((uint16)(arg))
# elif defined(MUSCLE_USE_MSVC_SWAP_FUNCTIONS)
#  define B_SWAP_INT64(arg)    _byteswap_uint64((uint64)(arg))
#  define B_SWAP_INT32(arg)    _byteswap_ulong((uint32)(arg))
#  define B_SWAP_INT16(arg)    _byteswap_ushort((uint16)(arg))
# elif defined(MUSCLE_USE_X86_INLINE_ASSEMBLY)
static inline uint16 MuscleX86SwapInt16(uint16 val)
{
#ifdef _MSC_VER
   __asm {
      mov ax, val;
      xchg al, ah;
      mov val, ax;
   };
#elif defined(MUSCLE_64_BIT_PLATFORM)
   __asm__ ("xchgb %h0, %b0" : "+Q" (val));
#else
   __asm__ ("xchgb %h0, %b0" : "+q" (val));
#endif
   return val;
}
static inline uint32 MuscleX86SwapInt32(uint32 val)
{
#ifdef _MSC_VER
   __asm {
      mov eax, val;
      bswap eax;
      mov val, eax;
   };
#else
   __asm__ ("bswap %0" : "+r" (val));
#endif
   return val;
}
static inline uint64 MuscleX86SwapInt64(uint64 val)
{
#ifdef _MSC_VER
   __asm {
      mov eax, DWORD PTR val;
      mov edx, DWORD PTR val + 4;
      bswap eax;
      bswap edx;
      mov DWORD PTR val, edx;
      mov DWORD PTR val + 4, eax;
   };
   return val;
#elif defined(MUSCLE_64_BIT_PLATFORM)
   __asm__ ("bswap %0" : "=r" (val) : "0" (val));
   return val;
#else
   return ((uint64)(MuscleX86SwapInt32((uint32)((val>>32)&0xFFFFFFFF))))|(((uint64)(MuscleX86SwapInt32((uint32)(val&0xFFFFFFFF))))<<32);
#endif
}
#  define B_SWAP_INT64(arg)    MuscleX86SwapInt64((uint64)(arg))
#  define B_SWAP_INT32(arg)    MuscleX86SwapInt32((uint32)(arg))
#  define B_SWAP_INT16(arg)    MuscleX86SwapInt16((uint16)(arg))
# else

// No assembly language available... so we'll use inline C

# if defined(__cplusplus)
#  define MUSCLE_INLINE inline
# else
#  define MUSCLE_INLINE static inline
# endif

MUSCLE_INLINE int64 B_SWAP_INT64(int64 arg)
{
   union {int64 _i64; uint8 _i8[8];} u1, u2;
   u1._i64   = arg;
   u2._i8[0] = u1._i8[7];
   u2._i8[1] = u1._i8[6];
   u2._i8[2] = u1._i8[5];
   u2._i8[3] = u1._i8[4];
   u2._i8[4] = u1._i8[3];
   u2._i8[5] = u1._i8[2];
   u2._i8[6] = u1._i8[1];
   u2._i8[7] = u1._i8[0];
   return u2._i64;
}
MUSCLE_INLINE int32 B_SWAP_INT32(int32 arg)
{
   union {int32 _i32; uint8 _i8[4];} u1, u2;
   u1._i32   = arg;
   u2._i8[0] = u1._i8[3];
   u2._i8[1] = u1._i8[2];
   u2._i8[2] = u1._i8[1];
   u2._i8[3] = u1._i8[0];
   return u2._i32;
}
MUSCLE_INLINE int16 B_SWAP_INT16(int16 arg)
{
   union {int16 _i16; uint8 _i8[2];} u1, u2;
   u1._i16   = arg;
   u2._i8[0] = u1._i8[1];
   u2._i8[1] = u1._i8[0];
   return u2._i16;
}
# endif
# if BYTE_ORDER == LITTLE_ENDIAN
#  define B_HOST_IS_LENDIAN 1
#  define B_HOST_IS_BENDIAN 0
#  define B_HOST_TO_LENDIAN_INT64(arg)  ((uint64)(arg))
#  define B_HOST_TO_LENDIAN_INT32(arg)  ((uint32)(arg))
#  define B_HOST_TO_LENDIAN_INT16(arg)  ((uint16)(arg))
#  define B_HOST_TO_BENDIAN_INT64(arg)  B_SWAP_INT64(arg)
#  define B_HOST_TO_BENDIAN_INT32(arg)  B_SWAP_INT32(arg)
#  define B_HOST_TO_BENDIAN_INT16(arg)  B_SWAP_INT16(arg)
#  define B_LENDIAN_TO_HOST_INT64(arg)  ((uint64)(arg))
#  define B_LENDIAN_TO_HOST_INT32(arg)  ((uint32)(arg))
#  define B_LENDIAN_TO_HOST_INT16(arg)  ((uint16)(arg))
#  define B_BENDIAN_TO_HOST_INT64(arg)  B_SWAP_INT64(arg)
#  define B_BENDIAN_TO_HOST_INT32(arg)  B_SWAP_INT32(arg)
#  define B_BENDIAN_TO_HOST_INT16(arg)  B_SWAP_INT16(arg)
# else
#  define B_HOST_IS_LENDIAN 0
#  define B_HOST_IS_BENDIAN 1
#  define B_HOST_TO_LENDIAN_INT64(arg)  B_SWAP_INT64(arg)
#  define B_HOST_TO_LENDIAN_INT32(arg)  B_SWAP_INT32(arg)
#  define B_HOST_TO_LENDIAN_INT16(arg)  B_SWAP_INT16(arg)
#  define B_HOST_TO_BENDIAN_INT64(arg)  ((uint64)(arg))
#  define B_HOST_TO_BENDIAN_INT32(arg)  ((uint32)(arg))
#  define B_HOST_TO_BENDIAN_INT16(arg)  ((uint16)(arg))
#  define B_LENDIAN_TO_HOST_INT64(arg)  B_SWAP_INT64(arg)
#  define B_LENDIAN_TO_HOST_INT32(arg)  B_SWAP_INT32(arg)
#  define B_LENDIAN_TO_HOST_INT16(arg)  B_SWAP_INT16(arg)
#  define B_BENDIAN_TO_HOST_INT64(arg)  ((uint64)(arg))
#  define B_BENDIAN_TO_HOST_INT32(arg)  ((uint32)(arg))
#  define B_BENDIAN_TO_HOST_INT16(arg)  ((uint16)(arg))
# endif
#endif /* !__BEOS__ && !__HAIKU__*/

/** Newer, architecture-safe float and double endian-ness swappers.  Note that for these
  * macros, the externalized value is expected to be stored in an int32 (for floats) or
  * in an int64 (for doubles).  See the FLOAT_TROUBLE and DOUBLE_TROUBLE comments elsewhere
  * in this header file for an explanation as to why.  --Jeremy 01/08/2007
  */

/* Yes, the memcpy() is necessary... mere pointer-casting plus assignment operations don't cut it under x86 */
static inline uint32 B_REINTERPRET_FLOAT_AS_INT32(float arg)   {uint32 r; memcpy(&r, &arg, sizeof(r)); return r;}
static inline float  B_REINTERPRET_INT32_AS_FLOAT(uint32 arg)  {float  r; memcpy(&r, &arg, sizeof(r)); return r;}
static inline uint64 B_REINTERPRET_DOUBLE_AS_INT64(double arg) {uint64 r; memcpy(&r, &arg, sizeof(r)); return r;}
static inline double B_REINTERPRET_INT64_AS_DOUBLE(uint64 arg) {double r; memcpy(&r, &arg, sizeof(r)); return r;}

#define B_HOST_TO_BENDIAN_IFLOAT(arg)  B_HOST_TO_BENDIAN_INT32(B_REINTERPRET_FLOAT_AS_INT32(arg))
#define B_BENDIAN_TO_HOST_IFLOAT(arg)  B_REINTERPRET_INT32_AS_FLOAT(B_BENDIAN_TO_HOST_INT32(arg))
#define B_HOST_TO_LENDIAN_IFLOAT(arg)  B_HOST_TO_LENDIAN_INT32(B_REINTERPRET_FLOAT_AS_INT32(arg))
#define B_LENDIAN_TO_HOST_IFLOAT(arg)  B_REINTERPRET_INT32_AS_FLOAT(B_LENDIAN_TO_HOST_INT32(arg))

#define B_HOST_TO_BENDIAN_IDOUBLE(arg) B_HOST_TO_BENDIAN_INT64(B_REINTERPRET_DOUBLE_AS_INT64(arg))
#define B_BENDIAN_TO_HOST_IDOUBLE(arg) B_REINTERPRET_INT64_AS_DOUBLE(B_BENDIAN_TO_HOST_INT64(arg))
#define B_HOST_TO_LENDIAN_IDOUBLE(arg) B_HOST_TO_LENDIAN_INT64(B_REINTERPRET_DOUBLE_AS_INT64(arg))
#define B_LENDIAN_TO_HOST_IDOUBLE(arg) B_REINTERPRET_INT64_AS_DOUBLE(B_LENDIAN_TO_HOST_INT64(arg))

/* Macro to turn a type code into a string representation.
 * (typecode) is the type code to get the string for
 * (buf) is a (char *) to hold the output string; it must be >= 5 bytes long.
 */
static inline void MakePrettyTypeCodeString(uint32 typecode, char *buf)
{
   uint32 i;  // keep separate, for C compatibility
   uint32 bigEndian = B_HOST_TO_BENDIAN_INT32(typecode);

   memcpy(buf, (const char *)&bigEndian, sizeof(bigEndian));
   buf[sizeof(bigEndian)] = '\0';
   for (i=0; i<sizeof(bigEndian); i++) if ((buf[i]<' ')||(buf[i]>'~')) buf[i] = '?';
}

#ifdef WIN32
# include <winsock2.h>  // this will bring in windows.h for us
#endif

#ifdef _MSC_VER
typedef UINT_PTR uintptr;   // Use these under MSVC so that the compiler
typedef INT_PTR  ptrdiff;   // doesn't give spurious warnings in /Wp64 mode
#else
# if defined(MUSCLE_64_BIT_PLATFORM)
typedef uint64 uintptr;
typedef int64 ptrdiff;
# else
typedef uint32 uintptr;
typedef int32 ptrdiff;
# endif
#endif

#ifdef __cplusplus
# include "syslog/SysLog.h"  /* for LogTime() */
#endif  /* __cplusplus */

/** Checks errno and returns true iff the last I/O operation
  * failed because it would have had to block otherwise.
  * NOTE:  Returns int so that it will compile even in C environments where no bool type is defined.
  */
static inline int PreviousOperationWouldBlock()
{
#ifdef WIN32
   return (WSAGetLastError() == WSAEWOULDBLOCK);
#else
   return (errno == EWOULDBLOCK);
#endif
}

/** Checks errno and returns true iff the last I/O operation
  * failed because it was interrupted by a signal or etc.
  * NOTE:  Returns int so that it will compile even in C environments where no bool type is defined.
  */
static inline int PreviousOperationWasInterrupted()
{
#ifdef WIN32
   return (WSAGetLastError() == WSAEINTR);
#else
   return (errno == EINTR);
#endif
}

/** Checks errno and returns true iff the last I/O operation
  * failed because of a transient condition which wasn't fatal to the socket.
  * NOTE:  Returns int so that it will compile even in C environments where no bool type is defined.
  */
static inline int PreviousOperationHadTransientFailure()
{
#ifdef WIN32
   int e = WSAGetLastError();
   return ((e == WSAEINTR)||(e == WSAENOBUFS));
#else
   int e = errno;
   return ((e == EINTR)||(e == ENOBUFS));
#endif
}

/** This function applies semi-standard logic to convert the return value
  * of a system I/O call and (errno) into a proper MUSCLE-standard return value.
  * (A MUSCLE-standard return value's semantics are:  Negative on error,
  * otherwise the return value is the number of bytes that were transferred)
  * @param origRet The return value of the original system call (e.g. to read()/write()/send()/recv())
  * @param maxSize The maximum number of bytes that the system call was permitted to send during that call.
  * @param blocking True iff the socket/file descriptor is in blocking I/O mode.  (Type is int for C compatibility -- it's really a boolean parameter)
  * @returns The system call's return value equivalent in MUSCLE return value semantics.
  */
static inline int32 ConvertReturnValueToMuscleSemantics(int origRet, uint32 maxSize, int blocking)
{
   int32 retForBlocking = ((origRet > 0)||(maxSize == 0)) ? origRet : -1;
   return blocking ? retForBlocking : ((origRet<0)&&((PreviousOperationWouldBlock())||(PreviousOperationHadTransientFailure()))) ? 0 : retForBlocking;
}

#ifdef __cplusplus
namespace muscle {
#endif

#if MUSCLE_TRACE_CHECKPOINTS > 0

/** Exposed as an implementation detail.  Please ignore! */
extern volatile uint32 * _muscleTraceValues;

/** Exposed as an implementation detail.  Please ignore! */
extern uint32 _muscleNextTraceValueIndex;

/** Sets the location of the trace-checkpoints array to store trace checkpoints into.
  * @param location A pointer to an array of at least (MUSCLE_TRACE_CHECKPOINTS) uint32s, or NULL.
  *                 If NULL (or if this function is never called), the default array will be used.
  */
void SetTraceValuesLocation(volatile uint32 * location);

/** Set this process's current trace value to (v).  This can be used as a primitive debugging tool, to determine
  * where this process was last seen executing -- useful for determining where the process is spinning at.
  * @note this function is a no-op if MUSCLE_TRACE_CHECKPOINTS is not defined to a value greater than zero.
  */
static inline void StoreTraceValue(uint32 v)
{
   _muscleTraceValues[_muscleNextTraceValueIndex] = v;  /* store the current value */
   _muscleNextTraceValueIndex                     = (_muscleNextTraceValueIndex+1)%MUSCLE_TRACE_CHECKPOINTS; /* move the pointer */
   _muscleTraceValues[_muscleNextTraceValueIndex] = MUSCLE_NO_LIMIT;  /* mark the next position with a special tag to show that it's next */
}

/** Returns a pointer to the first value in the trace-values array. */
static inline const volatile uint32 * GetTraceValues() {return _muscleTraceValues;}

/** A macro for automatically setting a trace checkpoint value based on current code location.
  * The value will be the two characters of the function or file name, left-shifted by 16 bits,
  * and then OR'd together with the current line number.  This should give the debugging person a
  * fairly good clue as to where the checkpoint was located, while still being very cheap to implement.
  *
  * @note This function will be a no-op unless MUSCLE_TRACE_CHECKPOINTS is defined to be greater than zero.
  */
#if defined(__GNUC__)
#define TCHECKPOINT                                   \
{                                                     \
   const char * d = __FUNCTION__;                     \
   StoreTraceValue((d[0]<<24)|(d[1]<<16)|(__LINE__)); \
}
#else
#define TCHECKPOINT                                   \
{                                                     \
   const char * d = __FILE__;                         \
   StoreTraceValue((d[0]<<24)|(d[1]<<16)|(__LINE__)); \
}
#endif

#else
/* no-op implementations for when we aren't using the trace facility */
static inline void SetTraceValuesLocation(volatile uint32 * location) {(void) location;}  /* named param is necessary for C compatibility */
static inline void StoreTraceValue(uint32 v) {(void) v;}  /* named param is necessary for C compatibility */
#define TCHECKPOINT {/* empty */}
#endif

#ifdef __cplusplus

/** This templated class is used as a "comparison callback" for sorting items in a Queue or Hashtable.
  * For many types, this default CompareFunctor template will do the job, but you also have the option of specifying
  * a different custom CompareFunctor for times when you want to sort in ways other than simply using the
  * less than and equals operators of the ItemType type.
  */
template <typename ItemType> class CompareFunctor
{
public:
   /**
    *  This is the signature of the type of callback function that you must pass
    *  into the Sort() method.  This function should work like strcmp(), returning
    *  a negative value if (item1) is less than item2, or zero if the items are
    *  equal, or a positive value if (item1) is greater than item2.
    *  The default implementation simply calls the muscleCompare() function.
    *  @param item1 The first item
    *  @param item2 The second item
    *  @param cookie A user-defined value that was passed in to the Sort() method.
    *  @return A value indicating which item is "larger", as defined above.
    */
   int Compare(const ItemType & item1, const ItemType & item2, void * /*cookie*/) const {return muscleCompare(item1, item2);}
};

/** Same as above, but used for pointers instead of direct items */
template <typename ItemType> class CompareFunctor<ItemType *>
{
public:
   int Compare(const ItemType * item1, const ItemType * item2, void * cookie) const {return CompareFunctor<ItemType>().Compare(*item1, *item2, cookie);}
};

/** For void pointers, we just compare the pointer values, since they can't be de-referenced. */
template<> class CompareFunctor<void *>
{
public:
   int Compare(void * s1, void * s2, void * /*cookie*/) const {return muscleCompare(s1, s2);}
};

/** We assume that (const char *)'s should always be compared using strcmp(). */
template<> class CompareFunctor<const char *>
{
public:
   int Compare(const char * s1, const char * s2, void * /*cookie*/) const {return strcmp(s1, s2);}
};

/** We assume that (const char *)'s should always be compared using strcmp(). */
template<> class CompareFunctor<char *>
{
public:
   int Compare(char * s1, char * s2, void * /*cookie*/) const {return strcmp(s1, s2);}
};

/** For cases where you really do want to just use a pointer as the key, instead
  * of cleverly trying to dereference the object it points to and sorting on that, you can specify this.
  */
template <typename PointerType> class PointerCompareFunctor
{
public:
   int Compare(PointerType s1, PointerType s2, void * /*cookie*/) const {return muscleCompare(s1, s2);}
};

/** Hash function for arbitrary data.  Note that the current implementation of this function
  * is MurmurHash2/Aligned, taken from http://murmurhash.googlepages.com/ and used as public domain code.
  * Thanks to Austin Appleby for the cool algorithm!
  * Note that these hash codes should not be passed outside of the
  * host that generated them, as different host architectures may give
  * different hash results for the same key data.
  * @param key Pointer to the data to hash
  * @param numBytes Number of bytes to hash start at (key)
  * @param seed An arbitrary number that affects the output values.  Defaults to zero.
  * @returns a 32-bit hash value corresponding to the hashed data.
  */
uint32 CalculateHashCode(const void * key, uint32 numBytes, uint32 seed = 0);

/** Same as HashCode(), but this version produces a 64-bit result.
  * This code is also part of MurmurHash2, written by Austin Appleby
  * Note that these hash codes should not be passed outside of the
  * host that generated them, as different host architectures may give
  * different hash results for the same key data.
  * @param key Pointer to the data to hash
  * @param numBytes Number of bytes to hash start at (key)
  * @param seed An arbitrary number that affects the output values.  Defaults to zero.
  * @returns a 32-bit hash value corresponding to the hashed data.
  */
uint64 CalculateHashCode64(const void * key, unsigned int numBytes, unsigned int seed = 0);

/** Convenience method; returns the hash code of the given data item.  Any POD type will do. 
  * @param val The value to calculate a hashcode for
  * @returns a hash code.
  */
template<typename T> inline uint32 CalculateHashCode(const T & val) {return CalculateHashCode(&val, sizeof(val));}

/** Convenience method; returns the 64-bit hash code of the given data item.  Any POD type will do. 
  * @param val The value to calculate a hashcode for
  * @returns a hash code.
  */
template<typename T> inline uint64 CalculateHashCode64(const T & val) {return CalculateHashCode64(&val, sizeof(val));}

/** This is a convenience function that will read through the passed-in byte
  * buffer and create a 32-bit checksum corresponding to its contents.
  * Note:  As of MUSCLE 5.22, this function is merely a synonym for CalculateHashCode().
  * @param buffer Pointer to the data to creat a checksum for.
  * @param numBytes Number of bytes that (buffer) points to.
  * @returns A 32-bit number based on the contents of the buffer.
  */
static inline uint32 CalculateChecksum(const void * buffer, uint32 numBytes) {return CalculateHashCode(buffer, numBytes);}

/** Convenience method:  Given a uint64, returns a corresponding 32-bit checksum value */
static inline uint32 CalculateChecksumForUint64(uint64 v) {uint64 le = B_HOST_TO_LENDIAN_INT64(v);   return CalculateChecksum(&le, sizeof(le));}

/** Convenience method:  Given a float, returns a corresponding 32-bit checksum value */
static inline uint32 CalculateChecksumForFloat(float v)   {uint32 le = (v==0.0f) ? 0 : B_HOST_TO_LENDIAN_IFLOAT(v); return CalculateChecksum(&le, sizeof(le));}  // yes, the special case for 0.0f IS necessary, because the sign-bit might be set.  :(

/** Convenience method:  Given a double, returns a corresponding 32-bit checksum value */
static inline uint32 CalculateChecksumForDouble(double v) {uint64 le = (v==0.0) ? 0 : B_HOST_TO_LENDIAN_IDOUBLE(v); return CalculateChecksum(&le, sizeof(le));}  // yes, the special case for 0.0 IS necessary, because the sign-bit might be set.  :(

/** This hashing functor type handles the trivial cases, where the KeyType is
 *  Plain Old Data that we can just feed directly into the CalculateHashCode() function.
 *  For more complicated key types, you should define a method in your KeyType class
 *  that looks like this:
 *      uint32 HashCode() const {return the_calculated_hash_code_for_this_object();}
 *  And that will be enough for the template magic to kick in and use MethodHashFunctor
 *  by default instead.  (See util/String.h for an example of this)
 */
template <class KeyType> class PODHashFunctor
{
public:
   uint32 operator()(const KeyType & x) const 
   {
#ifdef MUSCLE_USE_CPLUSPLUS11
      static_assert(!std::is_class<KeyType>::value, "PODHashFunctor cannot be used on class or struct objects, because the object's compiler-inserted padding bytes would be unitialized and therefore they would cause inconsistent hash-code generation.  Try adding a 'uint32 HashCode() const' method to the class/struct instead.");
      static_assert(!std::is_union<KeyType>::value, "PODHashFunctor cannot be used on union objects.");
#endif
      return CalculateHashCode(x);
   }
   bool AreKeysEqual(const KeyType & k1, const KeyType & k2) const {return (k1==k2);}
};

/** This hashing functor type calls HashCode() on the supplied object to retrieve the hash code. */
template <class KeyType> class MethodHashFunctor
{
public:
   uint32 operator()(const KeyType & x) const {return x.HashCode();}
   bool AreKeysEqual(const KeyType & k1, const KeyType & k2) const {return (k1==k2);}
};

/** This hashing functor type calls HashCode() on the supplied object to retrieve the hash code.  Used for pointers to key values. */
template <typename KeyType> class MethodHashFunctor<KeyType *>
{
public:
   uint32 operator()(const KeyType * x) const {return x->HashCode();}

   /** Note that when pointers are used as hash keys, we determine equality by comparing the objects
     * the pointers point to, NOT by just comparing the pointers themselves!
     */
   bool AreKeysEqual(const KeyType * k1, const KeyType * k2) const {return ((*k1)==(*k2));}
};

/** This macro can be used whenever you want to explicitly specify the default AutoChooseHashFunctorHelper functor for your type.  It's easier than remembering the tortured C++ syntax */
#define DEFAULT_HASH_FUNCTOR(type) AutoChooseHashFunctorHelper<type>::Type

namespace ugly_hashcode_method_sfinae_implementation
{
   // This code was from the example code at http://www.martinecker.com/wiki/index.php?title=Detecting_the_Existence_of_Member_Functions_at_Compile-Time
   // It is used by the AutoChooseHashFunctorHelper (below) to automatically choose the appropriate HashFunctor
   // type based on whether or not class given in the template argument has a "uint32 HashCode() const" method defined.

   typedef char yes;
   typedef char (&no)[2];
   template <typename T, uint32 (T::*f)() const> struct test_hashcode_wrapper {};

   // via SFINAE only one of these overloads will be considered
   template <typename T> yes hashcode_tester(test_hashcode_wrapper<T, &T::HashCode>*);
   template <typename T> no  hashcode_tester(...);

   template <typename T> struct test_hashcode_impl {static const bool value = sizeof(hashcode_tester<T>(0)) == sizeof(yes);};

   template <class T> struct has_hashcode_method : test_hashcode_impl<T> {};
   template <bool Condition, typename TrueResult, typename FalseResult> struct if_;
   template <typename TrueResult, typename FalseResult> struct if_<true,  TrueResult, FalseResult> {typedef TrueResult  result;};
   template <typename TrueResult, typename FalseResult> struct if_<false, TrueResult, FalseResult> {typedef FalseResult result;};
}

template<typename ItemType> class AutoChooseHashFunctorHelper
{
public:
   typedef typename ugly_hashcode_method_sfinae_implementation::if_<ugly_hashcode_method_sfinae_implementation::test_hashcode_impl<ItemType>::value, MethodHashFunctor<ItemType>, PODHashFunctor<ItemType> >::result Type;
};
template <typename ItemType> class AutoChooseHashFunctorHelper<ItemType *>
{
public:
   typedef typename ugly_hashcode_method_sfinae_implementation::if_<ugly_hashcode_method_sfinae_implementation::test_hashcode_impl<ItemType>::value, MethodHashFunctor<ItemType *>, PODHashFunctor<ItemType *> >::result Type;
};

/** This HashFunctor lets us use (const char *)'s as keys in a Hashtable.  They will be
  * hashed based on contents of the string they point to.
  */
template <> class PODHashFunctor<const char *>
{
public:
   /** Returns a hash code for the given C string.
     * @param str The C string to compute a hash code for.
     */
   uint32 operator () (const char * str) const {return CalculateHashCode(str, (uint32)strlen(str));}
   bool AreKeysEqual(const char * k1, const char * k2) const {return (strcmp(k1,k2)==0);}
};

template <> class AutoChooseHashFunctorHelper<const void *> {typedef PODHashFunctor<const void *> Type;};
template <> class AutoChooseHashFunctorHelper<char *>       {typedef PODHashFunctor<char *>       Type;};
template <> class AutoChooseHashFunctorHelper<void *>       {typedef PODHashFunctor<void *>       Type;};

// Hacked-in support for g++ 3.x compilers, that don't handle the AutoChooseHashFunctionHelper SFINAE magic properly
#if defined(__GNUC__) && (__GNUC__ <= 3)
# define AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(x) template <> class muscle::AutoChooseHashFunctorHelper<x> {public: typedef muscle::PODHashFunctor<x> Type;}
# define AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK_WITH_NAMESPACE(ns,x) }; namespace muscle {template <> class AutoChooseHashFunctorHelper<ns::x> {public: typedef PODHashFunctor<ns::x> Type;};}; namespace ns {
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(int);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(int8);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(uint8);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(int16);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(uint16);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(int32);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(uint32);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(int64);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(uint64);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(const char *);
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(float);
#else
# define AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(x)
# define AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK_WITH_NAMESPACE(ns,x)
#endif

#endif

// VC++6 and earlier can't handle partial template specialization, so
// they need some extra help at various places.  Lame....
#if defined(_MSC_VER)
# if (_MSC_VER < 1300)
#  define MUSCLE_USING_OLD_MICROSOFT_COMPILER 1  // VC++6 and earlier
# else
#  define MUSCLE_USING_NEW_MICROSOFT_COMPILER 1  // VC.net2004 and later
# endif
#endif

/** Given an ASCII representation of a non-negative number, returns that number as a uint64. */
uint64 Atoull(const char * str);

/** Similar to Atoll(), but handles negative numbers as well */
int64 Atoll(const char * str);

#ifdef __cplusplus
}; // end namespace muscle
#endif

#endif /* _MUSCLE_SUPPORT_H */
