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

#define MUSCLE_VERSION_STRING "8.20" /**< The current version of the MUSCLE distribution, expressed as an ASCII string */
#define MUSCLE_VERSION        82000  /**< Current version, expressed as decimal Mmmbb, where (M) is the number before the decimal point, (mm) is the number after the decimal point, and (bb) is reserved */

/*! \mainpage MUSCLE Documentation Page
 *
 * The MUSCLE API provides a robust, efficient, scalable, cross-platform, client-server messaging system
 * for network-distributed applications for Linux, MacOS/X, BSD, Windows, BeOS, and other operating
 * systems.  It can be compiled using any C++ compiler that supports C++03 or higher (although C++11 or
 * higher is preferred) and can run under any OS with a TCP/IP stack and BSD-style sockets API.
 *
 * MUSCLE allows an arbitrary number of client programs (each of which may be running on a separate computer and/or
 * under a different OS) to communicate with each other in a many-to-many structured-message-passing style.  It
 * employs a central server to which client programs may connect or disconnect at any time.  In addition to the client-server system, MUSCLE contains classes
 * to support peer-to-peer message-queue connections, as well as some handy miscellaneous utility
 * classes, all of which are documented here.
 *
 * All classes documented here should compile under most modern OS's with a modern C++ compiler.
 * (C++03 or newer is required, C++11 or newer is recommended)
 * Where platform-specific code is necessary, it has been provided (inside \#ifdef's) for various OS's.
 * C++ templates are used where appropriate; C++ exceptions are avoided in favor of returned error-codes.  The code is usable
 * in multithreaded environments, as long as you are careful.
 *
 * An examples-oriented tour of the various MUSCLE APIs can be found <a href="https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/site/index.html">here</a>.
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
# if !defined(MUSCLE_AVOID_CPLUSPLUS11) && (__cplusplus < 201100L) && !defined(_MSVC_LANG)
#  define MUSCLE_AVOID_CPLUSPLUS11
# endif
# ifndef MUSCLE_AVOID_CPLUSPLUS11
#  if !defined(MUSCLE_USE_PTHREADS) && !defined(MUSCLE_SINGLE_THREAD_ONLY) && !defined(MUSCLE_AVOID_CPLUSPLUS11_THREADS)
#   define MUSCLE_USE_CPLUSPLUS11_THREADS
#  endif
#  if defined(_MSC_VER) && (_MSC_VER < 1900)  // MSVC2013 and older don't implement constexpr, sigh
#   define MUSCLE_CONSTEXPR            /**< This tag indicates that the function or variable it decorates can be evaluated at compile time.  (Expands to keyword constexpr iff MUSCLE_AVOID_CPLUSPLUS11 is not defined) */
#  else
#   define MUSCLE_CONSTEXPR constexpr  /**< This tag indicates that the function or variable it decorates can be evaluated at compile time.  (Expands to keyword constexpr iff MUSCLE_AVOID_CPLUSPLUS11 is not defined) */
#   define MUSCLE_CONSTEXPR_IS_SUPPORTED /**< defined iff we are being compiled on a compiler (and in a compiler-mode) that supports the constexpr keyword */
#  endif
#  include <type_traits>  // for static_assert()
#  include <utility>      // for std::move()
#  if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__ < 7)
#   define MUSCLE_FINAL_CLASS          /**< If defined, this tag indicates that the class should not be subclassed.  (Expands to keyword "final" iff MUSCLE_AVOID_CPLUSPLUS11 is not defined) */ /* work-around for g++ bug #50811 */
#  else
#   define MUSCLE_FINAL_CLASS final   /**< This tag indicates that the class it decorates should not be subclassed.  (Expands to keyword "final" iff MUSCLE_AVOID_CPLUSPLUS11 is not defined) */
#  endif
# else
#  define MUSCLE_FINAL_CLASS          /**< This tag indicates that the class it decorates should not be subclassed.  (Expands to keyword "final" iff MUSCLE_AVOID_CPLUSPLUS11 is not defined) */
#  define MUSCLE_CONSTEXPR            /**< This tag indicates that the function or variable it decorates can be evaluated at compile time.  (Expands to keyword constexpr iff MUSCLE_AVOID_CPLUSPLUS11 is not defined) */
# endif
#else
# define NEW_H_NOT_AVAILABLE          /**< Defined iff C++ "new" include file isn't available (e.g. because we're on an ancient platform) */
#endif

#if !defined(MUSCLE_AVOID_CPLUSPLUS11) && !defined(MUSCLE_USE_CPLUSPLUS17) && ((__cplusplus >= 201703L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 201703L)))
# define MUSCLE_USE_CPLUSPLUS17
#endif

#if !defined(MUSCLE_AVOID_STDINT) && (defined(MUSCLE_AVOID_CPLUSPLUS11) || (defined(_MSC_VER) && (_MSC_VER < 1800)))
# define MUSCLE_AVOID_STDINT  // No sense trying to use cstdint on older compilers that we know do not provide it
#endif

#ifndef MUSCLE_AVOID_STDINT
# include <stdint.h>
# include <stddef.h>  // for ptrdiff_t when compiling as C code
# ifdef __cplusplus
#  include <cinttypes>
# else
#  include <inttypes.h>
# endif
#endif

// For non-C++-11 environments, we don't have the ability to make a class final, so we just define
// MUSCLE_FINAL_CLASS to expand to nothing, and leave it up to the user to know not to subclass the class.
#ifndef MUSCLE_FINAL_CLASS
# define MUSCLE_FINAL_CLASS
#endif

#ifndef MUSCLE_CONSTEXPR
# define MUSCLE_CONSTEXPR  /**< This tag indicates that the function or variable it decorates can be evaluated at compile time.  (Expands to keyword constexpr iff MUSCLE_AVOID_CPLUSPLUS11 is not defined) */
#endif

/* Borland C++ builder also runs under Win32, but it doesn't set this flag So we'd better set it ourselves. */
#if defined(__BORLANDC__) || defined(__WIN32__) || defined(_MSC_VER) || defined(_WIN32)
# ifndef WIN32
#  define WIN32 1
# endif
#endif

/* Win32 can't handle this stuff, it's too lame */
#ifdef WIN32
# define SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE  /** Defined iff we're not allowed to select() on file descriptor (e.g. because we're on Windows) */
# define UNISTD_H_NOT_AVAILABLE        /**< Defined iff <unistd.h> header isn't available (e.g. because we're on an ancient platform, or Windows) */
# ifndef _MSC_VER  /* 7/3/2006: Mika's patch allows VC++ to use newnothrow */
#  define NEW_H_NOT_AVAILABLE          /**< Defined iff C++ <new> header isn't available (e.g. because we're on an ancient platform) */
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
# define MUSCLE_AVOID_NEWNOTHROW /**< If defined, the newnothrow macro will expand to "new" rather than to "new (nothrow)" */
#endif

#ifndef newnothrow
# ifdef MUSCLE_AVOID_NEWNOTHROW
#  define newnothrow new            /**< newnothrow is a shortcut for "new (nothrow)", unless MUSCLE_AVOID_NEWNOTHROW is defined, in which case it becomes a synonym for plain old "new" */
# else
#  define newnothrow new (nothrow)  /**< newnothrow is a shortcut for "new (nothrow)", unless MUSCLE_AVOID_NEWNOTHROW is defined, in which case it becomes a synonym for plain old "new" */
# endif
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
// These macros are implementation details, please ignore them
# define ___MUSCLE_UNIQUE_NAME_AUX1(name, line) name##line
# define ___MUSCLE_UNIQUE_NAME_AUX2(name, line) ___MUSCLE_UNIQUE_NAME_AUX1(name, line)
#endif

/** This macro expands to a variable name which is (per-file) unique and unreferenceable.
  * This can be useful to help make sure the temporary variables in your macros don't
  * collide with each other.
  */
#define MUSCLE_UNIQUE_NAME ___MUSCLE_UNIQUE_NAME_AUX2(__uniqueName, __LINE__)

/** This macro declares an object on the stack with the specified argument(s). */
#define MDECLARE_ANONYMOUS_STACK_OBJECT(objType, ...) objType MUSCLE_UNIQUE_NAME = objType(__VA_ARGS__)

#ifdef MUSCLE_AVOID_ASSERTIONS
# define MASSERT(x,msg) /**< assertion macro.  Unless -DMUSCLE_AVOID_ASSERTIONS is specified, this macro will crash the program (with the error message in its second argument) if its first argument evaluates to false. */
#else
# define MASSERT(x,msg) {if(!(x)) MCRASH(msg)} /**< assertion macro.  Unless -DMUSCLE_AVOID_ASSERTIONS is specified, this macro will crash the program (with the error message in its second argument) if its first argument evaluates to false. */
#endif

/** This macro crashes the process with the specified error message. */
#define MCRASH(msg) {muscle::LogTime(muscle::MUSCLE_LOG_CRITICALERROR, "ASSERTION FAILED: (%s:%i) %s\n", __FILE__,__LINE__,msg); muscle::LogStackTrace(muscle::MUSCLE_LOG_CRITICALERROR); muscle::Crash();}

/** This macro immediately and rudely exits the process (by calling ExitWithoutCleanup(retVal)) after logging the specified critical error message. */
#define MEXIT(retVal,msg) {muscle::LogTime(muscle::MUSCLE_LOG_CRITICALERROR, "ASSERTION FAILED: (%s:%i) %s\n", __FILE__,__LINE__,msg); muscle::LogStackTrace(MUSCLE_LOG_CRITICALERROR); ExitWithoutCleanup(retVal);}

/** This macro logs an out-of-memory warning that includes the current filename and source-code line number.  WARN_OUT_OF_MEMORY() should be called whenever newnothrow or malloc() return NULL. */
#define MWARN_OUT_OF_MEMORY muscle::WarnOutOfMemory(__FILE__, __LINE__)

/** This macro logs an out-of-memory warning that includes the current filename and source-code line number, and then returns B_OUT_OF_MEMORY.  MRETURN_OUT_OF_MEMORY() may be called whenever newnothrow or malloc() return NULL inside a function that returns a status_t. */
#define MRETURN_OUT_OF_MEMORY {muscle::WarnOutOfMemory(__FILE__, __LINE__); return B_OUT_OF_MEMORY;}

/** This macro calls the specified status_t-returning function-call, and if it returns an error-value, it returns the error value. */
#define MRETURN_ON_ERROR(cmd) {const status_t the_return_value = (cmd); if (the_return_value.IsError()) return the_return_value;}

/** This macro invokes the MRETURN_OUT_OF_MEMORY macro if the argument is a NULL pointer. */
#define MRETURN_OOM_ON_NULL(ptr) {if (ptr==NULL) MRETURN_OUT_OF_MEMORY;}

/** This macro calls the specified status_t-returning function-call, and if it returns an error-value, writes an error message to the log */
#define MLOG_ON_ERROR(commandDesc, cmd) {const status_t the_return_value = (cmd); if (the_return_value.IsError()) LogTime(MUSCLE_LOG_ERROR, "%s:%i:  %s returned [%s]\n", __FILE__, __LINE__, commandDesc, the_return_value());}

/** This macro calls the specified status_t-returning function-call, and if it returns an error-value, writes an error message to the log and then returns the error-value */
#define MLOG_AND_RETURN_ON_ERROR(commandDesc, cmd) {const status_t the_return_value = (cmd); if (the_return_value.IsError()) {LogTime(MUSCLE_LOG_ERROR, "%s:%i:  %s returned [%s]\n", __FILE__, __LINE__, commandDesc, the_return_value()); return the_return_value;}}

/** This macro calls the specified status_t-returning function-call, and if it returns an error-value, prints an error message to stdout */
#define MPRINT_ON_ERROR(commandDesc, cmd) {const status_t the_return_value = (cmd); if (the_return_value.IsError()) printf("%s:%i:  %s returned [%s]\n", __FILE__, __LINE__, commandDesc, the_return_value());}

/** This macro calls the specified status_t-returning function-call, and if it returns an error-value, prints an error message to stdout */
#define MPRINT_AND_RETURN_ON_ERROR(commandDesc, cmd) {const status_t the_return_value = (cmd); if (the_return_value.IsError()) {printf("%s:%i:  %s returned [%s]\n", __FILE__, __LINE__, commandDesc, the_return_value()); return the_return_value;}}

/** This macro logs a warning message including the the current filename and source-code line number.  It can be useful for debugging/execution-path-tracing in environments without a debugger. */
#define MCHECKPOINT muscle::LogTime(muscle::MUSCLE_LOG_WARNING, "Reached checkpoint at %s:%i\n", __FILE__, __LINE__)

#if !defined(MUSCLE_64_BIT_PLATFORM)
# if defined(PTRDIFF_MAX)
#  if (PTRDIFF_MAX > 4294967296)
#   define MUSCLE_64_BIT_PLATFORM 1
#  endif
# elif defined(__osf__) || defined(__amd64__) || defined(__PPC64__) || defined(__x86_64__) || defined(_M_AMD64)
#  define MUSCLE_64_BIT_PLATFORM 1  // auto-define it if it wasn't defined in the Makefile
# endif
#endif

#if defined(__BEOS__) || defined(__HAIKU__)
# include <support/Errors.h>
# include <support/ByteOrder.h>  /* might as well use the real thing (and avoid complaints about duplication) */
# include <support/SupportDefs.h>
# include <support/TypeConstants.h>
# if !((defined(BONE))||(defined(BONE_VERSION))||(defined(__HAIKU__)))
#  define BEOS_OLD_NETSERVER
# endif
#else
   enum {
      CB_ERROR    = -1,        /**< For C programs: A value typically returned by a function or method with return type status_t, to indicate that it failed.  (When checking the value, it's better to check against B_NO_ERROR though, in case other failure values are defined in the future) */
      CB_NO_ERROR = 0,         /**< For C programs: The value returned by a function or method with return type status_t, to indicate that it succeeded with no errors. */
      CB_OK       = CB_NO_ERROR /**< For C programs: Synonym for CB_NO_ERROR */
   };
# ifdef __ATHEOS__
#  include </ainc/atheos/types.h>
# else
#  ifndef MUSCLE_TYPES_PREDEFINED  /* certain (ahem) projects already set these themselves... */
#   ifndef __cplusplus
#    define true                     1        /**< In C++, true is defined as non-zero, here as 1 */
#    define false                    0        /**< In C++, false is defined as zero */
#   endif
    typedef unsigned char           uchar;    /**< uchar is a synonym for "unsigned char" */
    typedef unsigned short          unichar;  /**< unichar is a synonym for "unsigned short" */
#   ifdef MUSCLE_AVOID_STDINT
     typedef signed char             int8;     /**< int8 is an 8-bit signed integer type */
     typedef unsigned char           uint8;    /**< uint8 is an 8-bit unsigned integer type */
     typedef short                   int16;    /**< int16 is a 16-bit signed integer type */
     typedef unsigned short          uint16;   /**< uint16 is a 16-bit unsigned integer type */
#    if defined(MUSCLE_64_BIT_PLATFORM)        /* some 64bit systems will have long=64-bit, int=32-bit */
      typedef int                    int32;    /**< int32 is a 32-bit signed integer type */
#     ifndef _UINT32
       typedef unsigned int          uint32;   /**< uint32 is a 32-bit unsigned integer type */
#      define _UINT32                          /**< Avoid conflict with typedef in OS/X Leopard system header */
#     endif
#    else
      typedef int                    int32;    /**< int32 is a 32-bit signed integer type */
#     ifndef _UINT32
       typedef unsigned int          uint32;   /**< uint32 is a 32-bit unsigned integer type */
#      define _UINT32                          /**< Avoid conflict with typedef in OS/X Leopard system header */
#     endif
#    endif
#    if defined(WIN32) && !defined(__GNUWIN32__)
      typedef __int64                int64;    /**< int64 is a 64-bit signed integer type */
      typedef unsigned __int64       uint64;   /**< uint64 is a 64-bit unsigned integer type */
#    elif __APPLE__
#     ifndef _UINT64
#      define _UINT64                          /**< Avoid conflict with typedef in OS/X Leopard system header */
       typedef long long              int64;   /**< int64 is a 64-bit signed integer type */    // these are what's expected in MacOS/X land, in both
       typedef unsigned long long     uint64;  /**< uint64 is a 64-bit unsigned integer type */ // 32-bit and 64-bit flavors.  C'est la vie, non?
#     endif
#    elif defined(MUSCLE_64_BIT_PLATFORM)
      typedef long                   int64;    /**< int64 is a 64-bit signed integer type */
      typedef unsigned long          uint64;   /**< uint64 is a 64-bit unsigned integer type */
#    else
      typedef long long              int64;    /**< int64 is a 64-bit signed integer type */
      typedef unsigned long long     uint64;   /**< uint64 is a 64-bit unsigned integer type */
#    endif
#   else
     // If stdint.h is available, we might as well use it
     typedef  int8_t    int8; /**< int8 is an 8-bit signed integer type */
     typedef uint8_t   uint8; /**< uint8 is an 8-bit unsigned integer type */
     typedef  int16_t  int16; /**< int16 is a 16-bit signed integer type */
     typedef uint16_t uint16; /**< uint16 is a 16-bit unsigned integer type */
     typedef  int32_t  int32; /**< int32 is a 32-bit signed integer type */
     typedef uint32_t uint32; /**< uint32 is a 32-bit unsigned integer type */
     typedef  int64_t  int64; /**< int64 is a 64-bit signed integer type */
     typedef uint64_t uint64; /**< uint64 is a 64-bit unsigned integer type */
#   endif
     typedef int32 c_status_t; /**< For C programs: This type indicates an expected value of either CB_NO_ERROR/CB_OK on success, or another value (often CB_ERROR) on failure. */
#   if defined(__cplusplus)
     namespace muscle {
        /** This class represents a return-value from a function or method that indicates success or failure.
          * It's implemented as a class instead of as a typedef or enum so that the compiler can provide
          * stricter compile-time type-checking and better error-reporting functionality.  When a function
          * wants to indicate that it succeeded, it should return B_NO_ERROR (or B_OK, which is a synonym).
          * If the function wants to indicate that it failed, it can return B_ERROR to indicate a
          * general/undescribed failure, or one of the other B_SOMETHING values (as listed in
          * support/MuscleSupport.h), or it can return B_ERROR("Some Error Description") if it wants to
          * describe its failure using an ad-hoc human-readable string.  In that last case, make sure the
          * string you pass in to B_ERROR is a compile-time constant, or in some other way * will remain
          * valid indefinitely, since the status_t object will keep only a (const char *) pointer to the
          * string, and therefore depends on that pointed-to char-array remaining valid.
          */
        class status_t
        {
        public:
           /** Default-constructor.  Creates a status_t representing success. */
           MUSCLE_CONSTEXPR status_t() : _desc(NULL) {/* empty */}

           /** Explicit Constructor.
             * param desc An optional human-description of the error.  If passed in as NULL (the default), the status will represent success.
             * @note The (desc) string is NOT copied: only the pointer is retained, so any non-NULL (desc) argument
             *       should be a compile-time-constant string!
             */
           MUSCLE_CONSTEXPR explicit status_t(const char * desc) : _desc(desc) {/* empty */}

           /** Copy constructor
             * @param rhs the status_t to make this object a copy of
             */
           MUSCLE_CONSTEXPR status_t(const status_t & rhs) : _desc(rhs._desc) {/* empty */}

           /** Comparison operator.  Returns true iff this object is equivalent to (rhs).
             * @param rhs the status_t to compare against
             */
           MUSCLE_CONSTEXPR bool operator ==(const status_t & rhs) const
           {
              return _desc ? ((rhs._desc)&&(strcmp(_desc, rhs._desc) == 0)) : (rhs._desc == NULL);
           }

#ifdef MUSCLE_USE_CPLUSPLUS17
           /** This method returns B_NO_ERROR iff both inputs are equal to B_NO_ERROR,
             * otherwise it returns the first non-B_NO_ERROR value available.  This method is
             * useful for chaining a ordered series of operations together and then
             * checking the aggregate result (e.g. status_t ret = a().And(b()).And(c()).And(d());)
             * @param rhs the second status_t to test this status_t against
             * @note Unlike with the | operator (below), the order-of-operations here is
             *       well-defined; however, this method is available only under C++17 or
             *       later, because older versions of C++ don't specify a well-defined
             *       order of operations for chained methods.
             * @note No short-circuiting is performed.
             */
           MUSCLE_CONSTEXPR status_t And(const status_t & rhs) const {return ((rhs.IsError())&&(!IsError())) ? rhs : *this;}
#endif

           /** Comparison operator.  Returns true iff this object has a different value than (rhs)
             * @param rhs the status_t to compare against
             */
           MUSCLE_CONSTEXPR bool operator !=(const status_t & rhs) const {return !(*this==rhs);}

           /** This operator returns B_NO_ERROR iff both inputs are equal to B_NO_ERROR,
             * otherwise it returns one of the non-B_NO_ERROR values.  This operator is
             * useful for aggregating a unordered series of operations together and
             * checking the aggregate result (e.g. status_t ret = a() | b() | c() | d())
             * @param rhs the second status_t to test this status_t against
             * @note Due to the way the | operator is defined in C++, the order of evaluations
             *       of the operations in the series in unspecified.  Also, no short-circuiting
             *       is performed; all operands will be evaluated regardless of their values.
             */
           MUSCLE_CONSTEXPR status_t operator | (const status_t & rhs) const {return ((rhs.IsError())&&(!IsError())) ? rhs : *this;}

           /** Sets this object equal to ((*this)|rhs).
             * @param rhs the second status_t to test this status_t against
             */
           status_t & operator |= (const status_t & rhs) {*this = ((*this)|rhs); return *this;}

           /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
           status_t & operator = (const status_t & rhs) {_desc = rhs._desc; return *this;}

           /** Returns "OK" if this status_t indicates success; otherwise returns the human-readable description
             * of the error this status_t indicates.
             */
           MUSCLE_CONSTEXPR const char * GetDescription() const {return IsOK() ? "OK" : _desc;}

           /** Convenience method -- a synonym for GetDescription() */
           MUSCLE_CONSTEXPR const char * operator()() const {return GetDescription();}

           /** Convenience method:  Returns true this object represents an ok/non-error status */
           MUSCLE_CONSTEXPR bool IsOK() const {return (_desc == NULL);}

           /** Convenience method:  Returns true iff this object represents an ok/non-error status
             * @param writeErrorTo If this object represents an error, the error will be copied into (writeErrorTo)
             * @note this allows for e.g. status_t ret; if ((func1().IsOK(ret))&&(func2().IsOK(ret))) {....} else return ret;
             */
           bool IsOK(status_t & writeErrorTo) const
           {
              const bool isOK = IsOK();
              if (isOK == false) writeErrorTo = *this;
              return isOK;
           }

           /** Convenience method:  Returns true iff this object represents an error-status */
           MUSCLE_CONSTEXPR bool IsError() const {return (_desc != NULL);}

           /** Convenience method:  Returns true iff this object represents an error-status
             * @param writeErrorTo If this object represents an error, the error will be copied into (writeErrorTo)
             * @note this allows for e.g. status_t ret; if ((func1().IsError(ret))||(func2().IsError(ret))) return ret;
             */
           bool IsError(status_t & writeErrorTo) const
           {
              const bool isError = IsError();
              if (isError) writeErrorTo = *this;
              return isError;
           }

           /** Returns a status_t with the given error-string.  (Added to allow e.g. B_ERROR("The Reason Why") syntax)
             * @param desc the error-string the returned status_t should have.  Should be a compile-time constant.
             * @note if this is called on a status_t that has a NULL error-string, then (desc) will be ignored and
             *       the returned status_t will also have a NULL error string.  That is to avoid doing the wrong thing
             *       if someone tries to do a B_NO_ERROR("Yay") or etc.
             */
           MUSCLE_CONSTEXPR status_t operator()(const char * desc) const {return status_t(_desc?desc:NULL);}

        private:
           const char * _desc;  // If non-NULL, we represent an error
        };

        // Basic/general status_t return codes
        const status_t B_NO_ERROR;       ///< This value is returned by a function or method that succeeded
        const status_t B_OK;             ///< This value is a synonym for B_NO_ERROR
        const status_t B_ERROR("Error"); ///< "Error": This value is returned by a function or method that errored out in a non-descript fashion
#       define B_ERRNO B_ERROR(strerror(GetErrno())) ///< Macro for return a B_ERROR with the current errno-string as its string-value
#       define B_ERRNUM(errnum) ((errnum==0)?B_NO_ERROR:B_ERROR(strerror(errnum))) ///< Macro for converting an errno-style return value (0==success, negative==error_value) result into a status_t

        // Some more-specific status_t return codes (for convenience, and to minimize the likelihood of
        // differently-phrased error strings for common types of reasons-for-failure)
        extern const status_t B_OUT_OF_MEMORY;  ///< "Out of Memory"  - we tried to allocate memory from the heap and got denied
        extern const status_t B_UNIMPLEMENTED;  ///< "Unimplemented"  - function is not implemented (for this OS?)
        extern const status_t B_ACCESS_DENIED;  ///< "Access Denied"  - we aren't allowed to do the thing we tried to do
        extern const status_t B_DATA_NOT_FOUND; ///< "Data not Found" - we couldn't find the data we were looking for
        extern const status_t B_FILE_NOT_FOUND; ///< "File not Found" - we couldn't find the file we were looking for
        extern const status_t B_BAD_ARGUMENT;   ///< "Bad Argument"   - one of the passed-in arguments didn't make sense
        extern const status_t B_BAD_DATA;       ///< "Bad Data"       - data we were trying to use was malformed
        extern const status_t B_BAD_OBJECT;     ///< "Bad Object"     - the object the method was called on is not in a usable state for this operation
        extern const status_t B_TIMED_OUT;      ///< "Timed Out"      - the operation took too long, so we gave up
        extern const status_t B_IO_ERROR;       ///< "I/O Error"      - an I/O operation failed
        extern const status_t B_LOCK_FAILED;    ///< "Lock Failed"    - an attempt to lock a shared resource (e.g. a Mutex) failed.
        extern const status_t B_TYPE_MISMATCH;  ///< "Type Mismatch"  - tried to fit a square block into a round hole
        extern const status_t B_ZLIB_ERROR;     ///< "ZLib Error"     - a zlib library-function reported an error
        extern const status_t B_SSL_ERROR;      ///< "SSL Error"      - an OpenSSL library-function reported an error
        extern const status_t B_LOGIC_ERROR;    ///< "Logic Error"    - internal logic has gone wrong somehow (bug?)
     };
#   endif  /* defined(__cplusplus) */
#  endif  /* !MUSCLE_TYPES_PREDEFINED */
# endif  /* !__ATHEOS__*/
#endif  /* __BEOS__ || __HAIKU__ */

#ifdef __cplusplus
template<typename T, size_t size> MUSCLE_CONSTEXPR uint32 ARRAYITEMS(T(&)[size]) {return (uint32) size;}  /**< Returns # of items in the 1D array.  Will error out at compile time if you try to call it with a pointer as an argument.  */
template<typename T, size_t rows, size_t cols> MUSCLE_CONSTEXPR uint32 ARRAYROWS(T(&)[rows][cols]) {return (uint32) rows;} /**< Returns # of "rows", along the first axis of the 2D array.  Will error out at compile time if you try to call it with a pointer as an argument.  */
template<typename T, size_t rows, size_t cols> MUSCLE_CONSTEXPR uint32 ARRAYCOLS(T(&)[rows][cols]) {return (uint32) cols;} /**< Returns # of "columns", along the second axis of the 2D array.  Will error out at compile time if you try to call it with a pointer as an argument.  */
#else
# define ARRAYITEMS(x) (sizeof(x)/sizeof(x[0]))  /**< Returns # of items in the array.  This primitive C-compatible implementation will return an incorrect value if called with a pointer as an argument. */
#endif
typedef void * muscleVoidPointer;  /**< Synonym for a (void *) -- it's a bit easier, syntax-wise, to use this type than (void *) directly in some cases. */

#if defined(__cplusplus) && !defined(MUSCLE_AVOID_CPLUSPLUS11)
// Let's try to catch any type-size errors at compile-time if we can
static_assert(sizeof(int8)   == 1, "sizeof(int8) != 1");
static_assert(sizeof(uint8)  == 1, "sizeof(uint8) != 1");
static_assert(sizeof(int16)  == 2, "sizeof(int16) != 2");
static_assert(sizeof(uint16) == 2, "sizeof(uint16) != 2");
static_assert(sizeof(int32)  == 4, "sizeof(int32) != 4");
static_assert(sizeof(uint32) == 4, "sizeof(uint32) != 4");
static_assert(sizeof(int64)  == 8, "sizeof(int64) != 8");
static_assert(sizeof(uint64) == 8, "sizeof(uint64) != 8");
static_assert(sizeof(float)  == 4, "sizeof(float) != 4");
static_assert(sizeof(float)  == 4, "sizeof(float) != 4");
static_assert(sizeof(double) == 8, "sizeof(double) != 8");
static_assert(sizeof(double) == 8, "sizeof(double) != 8");
#endif

#ifdef MUSCLE_AVOID_STDINT
/** Ugly platform-neutral macros for problematic sprintf()-format-strings */
# if defined(MUSCLE_64_BIT_PLATFORM)
#  define  INT32_FORMAT_SPEC_NOPERCENT "i"  /**< format-specifier string to pass in to printf() for an int32, without the percent sign */
#  define UINT32_FORMAT_SPEC_NOPERCENT "u"  /**< format-specifier string to pass in to printf() for a uint32, without the percent sign */
#  define XINT32_FORMAT_SPEC_NOPERCENT "x"  /**< format-specifier string to pass in to printf() for an int32 or uint32 that you want printed in hexadecimal, without the percent sign */
#  ifdef __APPLE__
#   define  INT64_FORMAT_SPEC_NOPERCENT "lli"  /**< format-specifier string to pass in to printf() for an int64, without the percent sign */
#   define UINT64_FORMAT_SPEC_NOPERCENT "llu"  /**< format-specifier string to pass in to printf() for a uint64, without the percent sign */
#   define XINT64_FORMAT_SPEC_NOPERCENT "llx"  /**< format-specifier string to pass in to printf() for an int64 or uint64 that you want printed in hexadecimal, without the percent sign */
#  elif defined(_MSC_VER) || defined(__MINGW64__)
#   define  INT64_FORMAT_SPEC_NOPERCENT "I64i"  /**< format-specifier string to pass in to printf() for an int64, without the percent sign */
#   define UINT64_FORMAT_SPEC_NOPERCENT "I64u"  /**< format-specifier string to pass in to printf() for a uint64, without the percent sign */
#   define XINT64_FORMAT_SPEC_NOPERCENT "I64x"  /**< format-specifier string to pass in to printf() for an int64 or uint64 that you want printed in hexadecimal, without the percent sign */
#  else
#   define  INT64_FORMAT_SPEC_NOPERCENT "li"  /**< format-specifier string to pass in to printf() for an int64, without the percent sign */
#   define UINT64_FORMAT_SPEC_NOPERCENT "lu"  /**< format-specifier string to pass in to printf() for a uint64, without the percent sign */
#   define XINT64_FORMAT_SPEC_NOPERCENT "lx"  /**< format-specifier string to pass in to printf() for an int64 or uint64 that you want printed in hexadecimal, without the percent sign */
#  endif
# else
#  define  INT32_FORMAT_SPEC_NOPERCENT "li"  /**< format-specifier string to pass in to printf() for an int32, without the percent sign */
#  define UINT32_FORMAT_SPEC_NOPERCENT "lu"  /**< format-specifier string to pass in to printf() for a uint32, without the percent sign */
#  define XINT32_FORMAT_SPEC_NOPERCENT "lx"  /**< format-specifier string to pass in to printf() for an int32 or uint32 that you want printed in hexadecimal, without the percent sign */
#  if defined(_MSC_VER) || defined(__MINGW32__)
#   define  INT64_FORMAT_SPEC_NOPERCENT "I64i"  /**< format-specifier string to pass in to printf() for an int64, without the percent sign */
#   define UINT64_FORMAT_SPEC_NOPERCENT "I64u"  /**< format-specifier string to pass in to printf() for a uint64, without the percent sign */
#   define XINT64_FORMAT_SPEC_NOPERCENT "I64x"  /**< format-specifier string to pass in to printf() for an int64 or uint64 that you want printed in hexadecimal, without the percent sign */
#  elif (defined(__BEOS__) && !defined(__HAIKU__)) || defined(__MWERKS__) || defined(__BORLANDC__)
#   define  INT64_FORMAT_SPEC_NOPERCENT "Li"  /**< format-specifier string to pass in to printf() for an int64, without the percent sign */
#   define UINT64_FORMAT_SPEC_NOPERCENT "Lu"  /**< format-specifier string to pass in to printf() for a uint64, without the percent sign */
#   define XINT64_FORMAT_SPEC_NOPERCENT "Lx"  /**< format-specifier string to pass in to printf() for an int64 or uint64 that you want printed in hexadecimal, without the percent sign */
#  else
#   define  INT64_FORMAT_SPEC_NOPERCENT "lli"  /**< format-specifier string to pass in to printf() for an int64, without the percent sign */
#   define UINT64_FORMAT_SPEC_NOPERCENT "llu"  /**< format-specifier string to pass in to printf() for a uint64, without the percent sign */
#   define XINT64_FORMAT_SPEC_NOPERCENT "llx"  /**< format-specifier string to pass in to printf() for an int64 or uint64 that you want printed in hexadecimal, without the percent sign */
#  endif
# endif
#else
// This version just uses the macros provided in inttypes.h
#  define  INT32_FORMAT_SPEC_NOPERCENT PRIi32 /**< format-specifier string to pass in to printf() for an int32, without the percent sign */
#  define UINT32_FORMAT_SPEC_NOPERCENT PRIu32 /**< format-specifier string to pass in to printf() for a uint32, without the percent sign */
#  define XINT32_FORMAT_SPEC_NOPERCENT PRIx32 /**< format-specifier string to pass in to printf() for an int32 or uint32 that you want printed in hexadecimal, without the percent sign */
#  define  INT64_FORMAT_SPEC_NOPERCENT PRIi64 /**< format-specifier string to pass in to printf() for a  int64, without the percent sign */
#  define UINT64_FORMAT_SPEC_NOPERCENT PRIu64 /**< format-specifier string to pass in to printf() for a uint64, without the percent sign */
#  define XINT64_FORMAT_SPEC_NOPERCENT PRIx64 /**< format-specifier string to pass in to printf() for an int64 or uint64 that you want printed in hexadecimal, without the percent sign */
#endif

#define  INT32_FORMAT_SPEC "%"  INT32_FORMAT_SPEC_NOPERCENT /**< format-specifier string to pass in to printf() for an int32, including the percent sign */
#define UINT32_FORMAT_SPEC "%" UINT32_FORMAT_SPEC_NOPERCENT /**< format-specifier string to pass in to printf() for a uint32, including the percent sign */
#define XINT32_FORMAT_SPEC "%" XINT32_FORMAT_SPEC_NOPERCENT /**< format-specifier string to pass in to printf() for an int32 or uint32 that you want printed in hexadecimal, including the percent sign */

#define  INT64_FORMAT_SPEC "%"  INT64_FORMAT_SPEC_NOPERCENT /**< format-specifier string to pass in to printf() for an int64, including the percent sign */
#define UINT64_FORMAT_SPEC "%" UINT64_FORMAT_SPEC_NOPERCENT /**< format-specifier string to pass in to printf() for a uint64, including the percent sign */
#define XINT64_FORMAT_SPEC "%" XINT64_FORMAT_SPEC_NOPERCENT /**< format-specifier string to pass in to printf() for an int64 or uint64 that you want printed in hexadecimal, including the percent sign */

/** Macro that returns a uint32 word out of the four ASCII characters in the supplied char array.  Used primarily to create 'what'-codes for Message objects (e.g. MAKETYPE("!Pc0") returns 558916400) */
#define MAKETYPE(x) ((((unsigned long)(x[0])) << 24) | \
                     (((unsigned long)(x[1])) << 16) | \
                     (((unsigned long)(x[2])) <<  8) | \
                     (((unsigned long)(x[3])) <<  0))

#if !defined(__BEOS__) && !defined(__HAIKU__)
/** Be-style message-field type codes.
  * I've calculated the integer equivalents for these codes
  * because g++ generates warnings about four-byte
  * constants when compiling under Linux --jaf
  */
enum {
   B_ANY_TYPE     = 1095653716, /**< 'ANYT' = wild card                                   */
   B_BOOL_TYPE    = 1112493900, /**< 'BOOL' = boolean (1 byte per bool)                   */
   B_DOUBLE_TYPE  = 1145195589, /**< 'DBLE' = double-precision float (8 bytes per double) */
   B_FLOAT_TYPE   = 1179406164, /**< 'FLOT' = single-precision float (4 bytes per float)  */
   B_INT64_TYPE   = 1280069191, /**< 'LLNG' = long long integer (8 bytes per int)         */
   B_INT32_TYPE   = 1280265799, /**< 'LONG' = long integer (4 bytes per int)              */
   B_INT16_TYPE   = 1397248596, /**< 'SHRT' = short integer (2 bytes per int)             */
   B_INT8_TYPE    = 1113150533, /**< 'BYTE' = byte integer (1 byte per int)               */
   B_MESSAGE_TYPE = 1297303367, /**< 'MSGG' = sub Message objects (reference counted)     */
   B_POINTER_TYPE = 1347310674, /**< 'PNTR' = pointers (will not be flattened)            */
   B_POINT_TYPE   = 1112559188, /**< 'BPNT' = Point objects (each Point has two floats)   */
   B_RECT_TYPE    = 1380270932, /**< 'RECT' = Rect objects (each Rect has four floats)    */
   B_STRING_TYPE  = 1129534546, /**< 'CSTR' = String objects (variable length)            */
   B_OBJECT_TYPE  = 1330664530, /**< 'OPTR' = Flattened user objects (obsolete)           */
   B_RAW_TYPE     = 1380013908, /**< 'RAWT' = Raw data (variable number of bytes)         */
   B_MIME_TYPE    = 1296649541, /**< 'MIME' = MIME strings (obsolete)                     */
   B_BITCHORD_TYPE= 1112818504  /**< 'BTCH' = BitChord type                               */
};
#endif

/** This one isn't defined by BeOS, so we have to enumerate it separately.                */
enum {
   B_TAG_TYPE     = 1297367367  /**< 'MTAG' = new for v2.00; for in-mem-only tags         */
};

/** This constant is used in various places to mean 'as much as you want'.  Its value is ((uint32)-1), aka ((2^32)-1) */
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
# define newnothrow_array(T, count) broken_gcc_newnothrow_array<T>(count)  /**< workaround wrapper for broken newnothrow T[count] under gcc 3.x */
#else
# define newnothrow_array(T, count) newnothrow T[count]                    /**< workaround wrapper for broken newnothrow T[count] under gcc 3.x */
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

#ifndef MUSCLE_AVOID_CPLUSPLUS11

/** Returns the smaller of the two arguments */
template<typename T> MUSCLE_CONSTEXPR T muscleMin(T arg1, T arg2) {return (arg1<arg2) ? arg1 : arg2;}

/** Returns the smallest of all of the arguments */
template<typename T1, typename ...T2> MUSCLE_CONSTEXPR T1 muscleMin(T1 arg1, T2... args) {return muscleMin(arg1, muscleMin(args...));}

/** Returns the larger of the two arguments */
template<typename T> MUSCLE_CONSTEXPR T muscleMax(T arg1, T arg2) {return (arg1>arg2) ? arg1 : arg2;}

/** Returns the largest of all of the arguments */
template<typename T1, typename ...T2> MUSCLE_CONSTEXPR T1 muscleMax(T1 arg1, T2... args) {return muscleMax(arg1, muscleMax(args...));}

#else

/** Returns the smaller of the two arguments */
template<typename T> inline MUSCLE_CONSTEXPR T muscleMin(T p1, T p2) {return (p1 < p2) ? p1 : p2;}

/** Returns the smallest of the three arguments */
template<typename T> inline MUSCLE_CONSTEXPR T muscleMin(T p1, T p2, T p3) {return muscleMin(p3, muscleMin(p1, p2));}

/** Returns the smallest of the four arguments */
template<typename T> inline MUSCLE_CONSTEXPR T muscleMin(T p1, T p2, T p3, T p4) {return muscleMin(p3, p4, muscleMin(p1, p2));}

/** Returns the smallest of the five arguments */
template<typename T> inline MUSCLE_CONSTEXPR T muscleMin(T p1, T p2, T p3, T p4, T p5) {return muscleMin(p3, p4, p5, muscleMin(p1, p2));}

/** Returns the larger of the two arguments */
template<typename T> inline MUSCLE_CONSTEXPR T muscleMax(T p1, T p2) {return (p1 < p2) ? p2 : p1;}

/** Returns the largest of the three arguments */
template<typename T> inline MUSCLE_CONSTEXPR T muscleMax(T p1, T p2, T p3) {return muscleMax(p3, muscleMax(p1, p2));}

/** Returns the largest of the four arguments */
template<typename T> inline MUSCLE_CONSTEXPR T muscleMax(T p1, T p2, T p3, T p4) {return muscleMax(p3, p4, muscleMax(p1, p2));}

/** Returns the largest of the five arguments */
template<typename T> inline MUSCLE_CONSTEXPR T muscleMax(T p1, T p2, T p3, T p4, T p5) {return muscleMax(p3, p4, p5, muscleMax(p1, p2));}

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

# ifndef DOXYGEN_SHOULD_IGNORE_THIS
namespace ugly_swapcontents_method_sfinae_implementation
{
   // This code was adapted from the example code at http://www.martinecker.com/wiki/index.php?title=Detecting_the_Existence_of_Member_Functions_at_Compile-Time
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
         if (&t1 == &t2) return;

#ifndef MUSCLE_AVOID_CPLUSPLUS11
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
      SwapContentsSwapper(T & t1, T & t2) {if (&t1 != &t2) t1.SwapContents(t2);}
   };

   template<typename ItemType> class AutoChooseSwapperHelper
   {
   public:
      typedef typename if_<test_swapcontents_impl<ItemType>::value, SwapContentsSwapper<ItemType>, PODSwapper<ItemType> >::result Type;
   };
}
# endif

/** Swaps the two arguments.
  * @param t1 First item to swap.  After this method returns, it will be equal to the old value of t2.
  * @param t2 Second item to swap.  After this method returns, it will be equal to the old value of t1.
  */
template<typename T> inline void muscleSwap(T & t1, T & t2) {typename ugly_swapcontents_method_sfinae_implementation::AutoChooseSwapperHelper<T>::Type swapper(t1,t2);}

#endif

/** Returns true iff (i) is a valid index into array (a)
  * @param i An index value
  * @param theArray an array of any type
  * @returns True iff i is non-negative AND less than ARRAYITEMS(theArray))
  */
template<typename T, int size> inline MUSCLE_CONSTEXPR bool muscleArrayIndexIsValid(int i, T (&theArray)[size]) {return ((theArray==theArray)&&(((unsigned int)i) < size));}

/** Convenience method for setting all items in the specified one-dimensional array to their default-constructed state (i.e. zero)
  * @param theArray an array of any type
  * @param t The object to set every item in the array equal to.  Defaults to a default-constructed object of the appropriate type.
  */
template<typename T, int size1> inline void muscleClearArray(T (&theArray)[size1], const T & t = GetDefaultObjectForType<T>()) {for (int i=0; i<size1; i++) theArray[i] = t;}

/** Convenience method for setting all items in the specified two-dimensional array to their default-constructed state (i.e. zero)
  * @param theArray an array of any type
  * @param t The object to set every item in the array equal to.  Defaults to a default-constructed object of the appropriate type.
  */
template<typename T, int size1, int size2> inline void muscleClearArray(T (&theArray)[size1][size2], const T & t = GetDefaultObjectForType<T>()) {for (int i=0; i<size1; i++) for (int j=0; j<size2; j++) theArray[i][j] = t;}

/** Convenience method for setting all items in the specified three-dimensional array to their default-constructed state (i.e. zero)
  * @param theArray an array of any type
  * @param t The object to set every item in the array equal to.  Defaults to a default-constructed object of the appropriate type.
  */
template<typename T, int size1, int size2, int size3> inline void muscleClearArray(T (&theArray)[size1][size2][size3], const T & t = GetDefaultObjectForType<T>()) {for (int i=0; i<size1; i++) for (int j=0; j<size2; j++) for (int k=0; k<size3; k++) theArray[i][j][k] = t;}

/** Returns the value nearest to (v) that is still in the range [lo, hi].
  * @param v A value
  * @param lo a minimum value
  * @param hi a maximum value
  * @returns The value in the range [lo, hi] that is closest to (v).
  */
template<typename T> inline MUSCLE_CONSTEXPR T muscleClamp(T v, T lo, T hi) {return (v < lo) ? lo : ((v > hi) ? hi : v);}

/** Returns true iff (v) is in the range [lo,hi].
  * @param v A value
  * @param lo A minimum value
  * @param hi A maximum value
  * @returns true iff (v >= lo) and (v <= hi)
  */
template<typename T> inline MUSCLE_CONSTEXPR bool muscleInRange(T v, T lo, T hi) {return ((v >= lo)&&(v <= hi));}

/** Returns -1 if arg1 is larger, or 1 if arg2 is larger, or 0 if they are equal.
  * @param arg1 First item to compare
  * @param arg2 Second item to compare
  * @returns -1 if (arg1<arg2), or 1 if (arg2<arg1), or 0 otherwise.
  */
template<typename T> inline MUSCLE_CONSTEXPR int muscleCompare(const T & arg1, const T & arg2) {return (arg2<arg1) ? 1 : ((arg1<arg2) ? -1 : 0);}

/** Returns the absolute value of (arg) */
template<typename T> inline MUSCLE_CONSTEXPR T muscleAbs(T arg) {return (arg<0)?(-arg):arg;}

/** Rounds the given float to the nearest integer value. */
inline MUSCLE_CONSTEXPR int muscleRintf(float f) {return (f>=0.0f) ? ((int)(f+0.5f)) : -((int)((-f)+0.5f));}

/** Returns -1 if the value is less than zero, +1 if it is greater than zero, or 0 otherwise. */
template<typename T> inline MUSCLE_CONSTEXPR int muscleSgn(T arg) {return (arg<0)?-1:((arg>0)?1:0);}

#endif  /* __cplusplus */

#if defined(__cplusplus) && (_MSC_VER >= 1400)
/** For MSVC, we provide CRT-friendly versions of these functions to avoid security warnings */
# define muscleSprintf  sprintf_s  /* Ooh, template magic! */
# define muscleSnprintf sprintf_s  /* Yes, sprintf_s is correct here! (this version is usable when template magic isn't possible) -jaf */
# define muscleStrcpy   strcpy_s   /* Ooh, template magic! */
static inline char * muscleStrncpy(char * to, const char * from, size_t maxLen) {(void) strcpy_s(to, maxLen, from); return to;}
static inline FILE * muscleFopen(const char * path, const char * mode) {FILE * fp; return (fopen_s(&fp, path, mode) == 0) ? fp : NULL;}
#else
/** Other OS's can use the usual functions instead. */
# define muscleSprintf  sprintf   /**< On Windows, this expands to sprintf_s to avoid security warnings; on other OS's it expands to plain old sprintf */
# define muscleSnprintf snprintf  /**< On Windows, this expands to snprintf_s to avoid security warnings; on other OS's it expands to plain old snprintf */
# define muscleStrcpy   strcpy    /**< On Windows, this expands to strcpy_s to avoid security warnings; on other OS's it expands to plain old strcpy */
# define muscleStrncpy  strncpy   /**< On Windows, this expands to strncpy_s to avoid security warnings; on other OS's it expands to plain old strncpy */
# define muscleFopen    fopen     /**< On Windows, this expands to fopen_s to avoid security warnings; on other OS's it expands to plain old fopen */
#endif

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
    #elif defined( __EMSCRIPTEN__ )
      #include <endian.h>
    #else
      #define LITTLE_ENDIAN   1234    /**< least-significant byte first (vax, pc) */
      #define BIG_ENDIAN      4321    /**< most-significant byte first (IBM, net) */

      #if defined(vax) || defined(ns32000) || defined(sun386) || defined(i386) || \
              defined(__i386) || defined(__ia64) || \
              defined(MIPSEL) || defined(_MIPSEL) || defined(BIT_ZERO_ON_RIGHT) || \
              defined(__alpha__) || defined(__alpha) || defined(__CYGWIN__) || \
              defined(_M_IX86) || defined(_M_AMD64) || defined(__GNUWIN32__) || defined(__LITTLEENDIAN__) || \
              defined(__MINGW32__) || defined(__MINGW64__) || \
              defined(_M_ARM) || defined(_M_ARM64) || \
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

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
# define B_HOST_TO_BENDIAN_FLOAT(arg) B_HOST_TO_BENDIAN_FLOAT_was_removed_use_B_HOST_TO_BENDIAN_IFLOAT_instead___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
# define B_HOST_TO_LENDIAN_FLOAT(arg) B_HOST_TO_LENDIAN_FLOAT_was_removed_use_B_HOST_TO_LENDIAN_IFLOAT_instead___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
# define B_BENDIAN_TO_HOST_FLOAT(arg) B_BENDIAN_TO_HOST_FLOAT_was_removed_use_B_BENDIAN_TO_HOST_IFLOAT_instead___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
# define B_LENDIAN_TO_HOST_FLOAT(arg) B_LENDIAN_TO_HOST_FLOAT_was_removed_use_B_LENDIAN_TO_HOST_IFLOAT_instead___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
# define B_SWAP_FLOAT(arg)            B_SWAP_FLOAT_was_removed___See_the_FLOAT_TROUBLE_comment_in_support_MuscleSupport_h_for_details.
#endif

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

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
# define B_HOST_TO_BENDIAN_DOUBLE(arg) B_HOST_TO_BENDIAN_DOUBLE_was_removed_use_B_HOST_TO_BENDIAN_IDOUBLE_instead___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
# define B_HOST_TO_LENDIAN_DOUBLE(arg) B_HOST_TO_LENDIAN_DOUBLE_was_removed_use_B_HOST_TO_LENDIAN_IDOUBLE_instead___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
# define B_BENDIAN_TO_HOST_DOUBLE(arg) B_BENDIAN_TO_HOST_DOUBLE_was_removed_use_B_BENDIAN_TO_HOST_IDOUBLE_instead___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
# define B_LENDIAN_TO_HOST_DOUBLE(arg) B_LENDIAN_TO_HOST_DOUBLE_was_removed_use_B_LENDIAN_TO_HOST_IDOUBLE_instead___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
# define B_SWAP_DOUBLE(arg)            B_SWAP_DOUBLE_was_removed___See_the_DOUBLE_TROUBLE_comment_in_support_MuscleSupport_h_for_details_
#endif

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
#  define B_SWAP_INT64(arg)    MusclePowerPCSwapInt64((uint64)(arg))  /**< Given a uint64, returns its endian-swapped equivalent */
#  define B_SWAP_INT32(arg)    MusclePowerPCSwapInt32((uint32)(arg))  /**< Given a uint32, returns its endian-swapped equivalent */
#  define B_SWAP_INT16(arg)    MusclePowerPCSwapInt16((uint16)(arg))  /**< Given a uint16, returns its endian-swapped equivalent */
# elif defined(MUSCLE_USE_MSVC_SWAP_FUNCTIONS)
#  define B_SWAP_INT64(arg)    _byteswap_uint64((uint64)(arg))        /**< Given a uint64, returns its endian-swapped equivalent */
#  define B_SWAP_INT32(arg)    _byteswap_ulong((uint32)(arg))         /**< Given a uint32, returns its endian-swapped equivalent */
#  define B_SWAP_INT16(arg)    _byteswap_ushort((uint16)(arg))        /**< Given a uint16, returns its endian-swapped equivalent */
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
#  define MUSCLE_INLINE inline          /**< Expands to "inline" for C++ code, or to "static inline" for C code */
# else
#  define MUSCLE_INLINE static inline   /**< Expands to "inline" for C++ code, or to "static inline" for C code */
# endif

/** Given a 64-bit integer, returns its endian-swapped counterpart */
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

/** Given a 32-bit integer, returns its endian-swapped counterpart */
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

/** Given a 16-bit integer, returns its endian-swapped counterpart */
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
#  define B_HOST_IS_LENDIAN 1                              /**< Set to 1 if native CPU is little-endian; set to 0 if native CPU is big-endian. */
#  define B_HOST_IS_BENDIAN 0                              /**< Set to 1 if native CPU is big-endian; set to 0 if native CPU is little-endian. */
#  define B_HOST_TO_LENDIAN_INT64(arg)  ((uint64)(arg))    /**< Given a uint64 or int64 in native-endian format, returns its equivalent little-endian value. */
#  define B_HOST_TO_LENDIAN_INT32(arg)  ((uint32)(arg))    /**< Given a uint32 or int32 in native-endian format, returns its equivalent little-endian value. */
#  define B_HOST_TO_LENDIAN_INT16(arg)  ((uint16)(arg))    /**< Given a uint16 or int16 in native-endian format, returns its equivalent little-endian value. */
#  define B_HOST_TO_BENDIAN_INT64(arg)  B_SWAP_INT64(arg)  /**< Given a uint64 or int64 in native-endian format, returns its equivalent big-endian value. */
#  define B_HOST_TO_BENDIAN_INT32(arg)  B_SWAP_INT32(arg)  /**< Given a uint32 or int32 in native-endian format, returns its equivalent big-endian value. */
#  define B_HOST_TO_BENDIAN_INT16(arg)  B_SWAP_INT16(arg)  /**< Given a uint16 or int16 in native-endian format, returns its equivalent big-endian value. */
#  define B_LENDIAN_TO_HOST_INT64(arg)  ((uint64)(arg))    /**< Given a uint64 or int64 in little-endian format, returns its equivalent native-endian value. */
#  define B_LENDIAN_TO_HOST_INT32(arg)  ((uint32)(arg))    /**< Given a uint32 or int32 in little-endian format, returns its equivalent native-endian value. */
#  define B_LENDIAN_TO_HOST_INT16(arg)  ((uint16)(arg))    /**< Given a uint16 or int16 in little-endian format, returns its equivalent native-endian value. */
#  define B_BENDIAN_TO_HOST_INT64(arg)  B_SWAP_INT64(arg)  /**< Given a uint64 or int64 in big-endian format, returns its equivalent native-endian value. */
#  define B_BENDIAN_TO_HOST_INT32(arg)  B_SWAP_INT32(arg)  /**< Given a uint32 or int32 in big-endian format, returns its equivalent native-endian value. */
#  define B_BENDIAN_TO_HOST_INT16(arg)  B_SWAP_INT16(arg)  /**< Given a uint16 or int16 in big-endian format, returns its equivalent native-endian value. */
# else
#  define B_HOST_IS_LENDIAN 0                              /**< Set to 1 if native CPU is little-endian; set to 0 if native CPU is big-endian. */
#  define B_HOST_IS_BENDIAN 1                              /**< Set to 1 if native CPU is big-endian; set to 0 if native CPU is little-endian. */
#  define B_HOST_TO_LENDIAN_INT64(arg)  B_SWAP_INT64(arg)  /**< Given a uint64 or int64 in native-endian format, returns its equivalent little-endian value. */
#  define B_HOST_TO_LENDIAN_INT32(arg)  B_SWAP_INT32(arg)  /**< Given a uint32 or int32 in native-endian format, returns its equivalent little-endian value. */
#  define B_HOST_TO_LENDIAN_INT16(arg)  B_SWAP_INT16(arg)  /**< Given a uint16 or int16 in native-endian format, returns its equivalent little-endian value. */
#  define B_HOST_TO_BENDIAN_INT64(arg)  ((uint64)(arg))    /**< Given a uint64 or int64 in native-endian format, returns its equivalent big-endian value. */
#  define B_HOST_TO_BENDIAN_INT32(arg)  ((uint32)(arg))    /**< Given a uint32 or int32 in native-endian format, returns its equivalent big-endian value. */
#  define B_HOST_TO_BENDIAN_INT16(arg)  ((uint16)(arg))    /**< Given a uint16 or int16 in native-endian format, returns its equivalent big-endian value. */
#  define B_LENDIAN_TO_HOST_INT64(arg)  B_SWAP_INT64(arg)  /**< Given a uint64 or int64 in little-endian format, returns its equivalent native-endian value. */
#  define B_LENDIAN_TO_HOST_INT32(arg)  B_SWAP_INT32(arg)  /**< Given a uint32 or int32 in little-endian format, returns its equivalent native-endian value. */
#  define B_LENDIAN_TO_HOST_INT16(arg)  B_SWAP_INT16(arg)  /**< Given a uint16 or int16 in little-endian format, returns its equivalent native-endian value. */
#  define B_BENDIAN_TO_HOST_INT64(arg)  ((uint64)(arg))    /**< Given a uint64 or int64 in big-endian format, returns its equivalent native-endian value. */
#  define B_BENDIAN_TO_HOST_INT32(arg)  ((uint32)(arg))    /**< Given a uint32 or int32 in big-endian format, returns its equivalent native-endian value. */
#  define B_BENDIAN_TO_HOST_INT16(arg)  ((uint16)(arg))    /**< Given a uint16 or int16 in big-endian format, returns its equivalent native-endian value. */
# endif
#endif /* !__BEOS__ && !__HAIKU__*/

/** Newer, architecture-safe float and double endian-ness swappers.  Note that for these
  * macros, the externalized value is expected to be stored in an int32 (for floats) or
  * in an int64 (for doubles).  See the FLOAT_TROUBLE and DOUBLE_TROUBLE comments elsewhere
  * in this header file for an explanation as to why.  --Jeremy 01/08/2007
  */

/** Safely reinterprets a floating-point value to store its bit-pattern in a uint32.  (Note that mere casting isn't safe under x86!)
  * @param arg The 32-bit floating point value to reinterpret
  * @returns a uint32 whose 32-bit bit-pattern is the same as the bits in (arg)
  */
static inline uint32 B_REINTERPRET_FLOAT_AS_INT32(float arg)   {uint32 r; memcpy(&r, &arg, sizeof(r)); return r;}

/** Reinterprets the bit-pattern of a uint32 as a floating-point value.  (Note that mere casting isn't safe under x86!)
  * @param arg The uint32 value whose contents are known to represent the bits of a 32-bit floating-point value.
  * @returns The original floating point value that (arg) is equivalent to.
  */
static inline float  B_REINTERPRET_INT32_AS_FLOAT(uint32 arg)  {float  r; memcpy(&r, &arg, sizeof(r)); return r;}

/** Safely reinterprets a double-precision floating-point value to store its bit-pattern in a uint64.  (Note that mere casting isn't safe under x86!)
  * @param arg The 64-bit floating point value to reinterpret
  * @returns a uint64 whose 64-bit bit-pattern is the same as the bits in (arg)
  */
static inline uint64 B_REINTERPRET_DOUBLE_AS_INT64(double arg) {uint64 r; memcpy(&r, &arg, sizeof(r)); return r;}

/** Reinterprets the bit-pattern of a uint64 as a double-precision floating-point value.  (Note that mere casting isn't safe under x86!)
  * @param arg The uint64 value whose contents are known to represent the bits of a 64-bit floating-point value.
  * @returns The original double-precision point value that (arg) is equivalent to.
  */
static inline double B_REINTERPRET_INT64_AS_DOUBLE(uint64 arg) {double r; memcpy(&r, &arg, sizeof(r)); return r;}

/** Given a 32-bit floating point value in native-endian form, returns a 32-bit integer contains the floating-point value's bits in big-endian format. */
#define B_HOST_TO_BENDIAN_IFLOAT(arg)  B_HOST_TO_BENDIAN_INT32(B_REINTERPRET_FLOAT_AS_INT32(arg))

/** Given a 32-bit integer representing a floating point value in big-endian format, returns the floating point value in native-endian form. */
#define B_BENDIAN_TO_HOST_IFLOAT(arg)  B_REINTERPRET_INT32_AS_FLOAT(B_BENDIAN_TO_HOST_INT32(arg))

/** Given a 32-bit floating point value in native-endian form, returns a 32-bit integer contains the floating-point value's bits in little-endian format. */
#define B_HOST_TO_LENDIAN_IFLOAT(arg)  B_HOST_TO_LENDIAN_INT32(B_REINTERPRET_FLOAT_AS_INT32(arg))

/** Given a 32-bit integer representing a floating point value in little-endian format, returns the floating point value in native-endian form. */
#define B_LENDIAN_TO_HOST_IFLOAT(arg)  B_REINTERPRET_INT32_AS_FLOAT(B_LENDIAN_TO_HOST_INT32(arg))

/** Given a 64-bit floating point value in native-endian form, returns a 64-bit integer contains the floating-point value's bits in big-endian format. */
#define B_HOST_TO_BENDIAN_IDOUBLE(arg) B_HOST_TO_BENDIAN_INT64(B_REINTERPRET_DOUBLE_AS_INT64(arg))

/** Given a 64-bit integer representing a floating point value in big-endian format, returns the double-precision floating point value in native-endian form. */
#define B_BENDIAN_TO_HOST_IDOUBLE(arg) B_REINTERPRET_INT64_AS_DOUBLE(B_BENDIAN_TO_HOST_INT64(arg))

/** Given a 64-bit floating point value in native-endian format, returns a 64-bit integer contains the floating-point value's bits in little-endian format. */
#define B_HOST_TO_LENDIAN_IDOUBLE(arg) B_HOST_TO_LENDIAN_INT64(B_REINTERPRET_DOUBLE_AS_INT64(arg))

/** Given a 64-bit integer representing a floating point value in little-endian format, returns the double-precision floating point value in native-endian form. */
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

#ifdef MUSCLE_AVOID_STDINT
# ifdef _MSC_VER
typedef UINT_PTR uintptr;   // Use these under MSVC so that the compiler
typedef INT_PTR  ptrdiff;   // doesn't give spurious warnings in /Wp64 mode
# else
#  if defined(MUSCLE_64_BIT_PLATFORM)
typedef uint64 uintptr;  /**< uintptr is an unsigned integer type that is guaranteed to be able to hold a pointer value for the native CPU, without any data loss */
typedef int64 ptrdiff;   /**< ptrdiff is a signed integer type that is guaranteed to be able to hold the difference between two pointer values for the native CPU, without any data loss */
#  else
typedef uint32 uintptr;  /**< uintptr is an unsigned integer type that is guaranteed to be able to hold a pointer value for the native CPU, without any data loss */
typedef int32 ptrdiff;   /**< ptrdiff is a signed integer type that is guaranteed to be able to hold the difference between two pointer values for the native CPU, without any data loss */
#  endif
# endif
#else
typedef uintptr_t uintptr; /**< uintptr is an unsigned integer type that is guaranteed to be able to hold a pointer value for the native CPU, without any data loss */
typedef ptrdiff_t ptrdiff; /**< ptrdiff is a signed integer type that is guaranteed to be able to hold the difference between two pointer values for the native CPU, without any data loss */
#endif

#ifdef __cplusplus
# include "syslog/SysLog.h"  /* for LogTime() */
#endif  /* __cplusplus */

/** Platform-neutral interface to reading errno -- returns WSAGetLastError() on Windows, or errno on other OS's */
static inline int GetErrno()
{
#ifdef WIN32
   return WSAGetLastError();
#else
   return errno;
#endif
}

/** Platform-neutral interface to setting errno -- calls WSASetLastError() on Windows, or sets errno on other OS's
  * @param e The new value to set the errno variable to
  */
static inline void SetErrno(int e)
{
#ifdef WIN32
   WSASetLastError(e);
#else
   errno = e;
#endif
}

/** Checks errno and returns true iff the last I/O operation
  * failed because it would have had to block otherwise.
  * NOTE:  Returns int so that it will compile even in C environments where no bool type is defined.
  */
static inline int PreviousOperationWouldBlock()
{
   const int e = GetErrno();
#ifdef WIN32
   return (e == WSAEWOULDBLOCK);
#else
   return (e == EWOULDBLOCK);
#endif
}

/** Checks errno and returns true iff the last I/O operation
  * failed because it was interrupted by a signal or etc.
  * NOTE:  Returns int so that it will compile even in C environments where no bool type is defined.
  */
static inline int PreviousOperationWasInterrupted()
{
   const int e = GetErrno();
#ifdef WIN32
   return (e == WSAEINTR);
#else
   return (e == EINTR);
#endif
}

/** Checks errno and returns true iff the last I/O operation
  * failed because of a transient condition which wasn't fatal to the socket.
  * NOTE:  Returns int so that it will compile even in C environments where no bool type is defined.
  */
static inline int PreviousOperationHadTransientFailure()
{
   const int e = GetErrno();
#ifdef WIN32
   return ((e == WSAEINTR)||(e == WSAENOBUFS));
#else
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
static inline int32 ConvertReturnValueToMuscleSemantics(long origRet, uint32 maxSize, int blocking)
{
   int32 retForBlocking = ((origRet > 0)||(maxSize == 0)) ? (int32)origRet : -1;
   return blocking ? retForBlocking : ((origRet<0)&&((PreviousOperationWouldBlock())||(PreviousOperationHadTransientFailure()))) ? 0 : retForBlocking;
}

#ifdef __cplusplus
namespace muscle {
class DataNode;  // FogBugz #9816 tweakage
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
// forward declarations
extern void Crash();
extern void ExitWithoutCleanup(int);
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

/** A macro for automatically setting a trace checkpoint value based on current code location.
  * The value will be the two characters of the function or file name, left-shifted by 16 bits,
  * and then OR'd together with the current line number.  This should give the debugging person a
  * fairly good clue as to where the checkpoint was located, while still being very cheap to implement.
  *
  * @note This function will be a no-op unless MUSCLE_TRACE_CHECKPOINTS is defined to be greater than zero.
  */
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
   int Compare(const ItemType & item1, const ItemType & item2, void * cookie) const {(void) cookie; return muscleCompare(item1, item2);}
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
  * @param key Pointer to the data to hash
  * @param numBytes Number of bytes to hash start at (key)
  * @param seed An arbitrary number that affects the output values.  Defaults to zero.
  * @returns a 32-bit hash value corresponding to the hashed data.
  */
uint32 CalculateHashCode(const void * key, uint32 numBytes, uint32 seed = 0);

/** Same as HashCode(), but this version produces a 64-bit result.
  * This code is also part of MurmurHash2, written by Austin Appleby
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

/** Convenience method:  Returns the Euclidean modulo of the given value for the given divisor.
  * @param value the value to calculate the Euclidean-modulo of
  * @param divisor the divisor to use in the calculation.  Must not be zero.
  * @note For non-negative values of (value), this function behaves the same as (value%divisor).
  *       For negative values of (value), this function behaves differently
  *       in that e.g. EuclideanModulo(-1,d) will return (d-1) rather than -1.  This is
  *       arguably more useful for cyclic-sequence applications, as there will not be
  *       any anomalies in the resulting values as (value) transitions between positive and negative.
  */
static inline uint32 EuclideanModulo(int32 value, uint32 divisor)
{
   // Derived from the code posted at https://stackoverflow.com/a/51959866/131930
   return (value < 0) ? ((divisor-1)-((-1-value)%divisor)) : (value%divisor);
}

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
#ifndef MUSCLE_AVOID_CPLUSPLUS11
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

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
namespace ugly_hashcode_method_sfinae_implementation
{
   // This code was adapted from the example code at http://www.martinecker.com/wiki/index.php?title=Detecting_the_Existence_of_Member_Functions_at_Compile-Time
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
#endif

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
 AUTOCHOOSE_LEGACY_PRIMITIVE_KEY_TYPE_HACK(DataNode *);  // FogBugz #9816 tweakage --jaf
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

/** Given an ASCII decimal representation of a non-negative number, returns that number as a uint64. */
uint64 Atoull(const char * str);

/** Similar to Atoull(), but handles negative numbers as well */
int64 Atoll(const char * str);

/** Given an ASCII hexadecimal representation of a non-negative number (with or without the optional "0x" prefix), returns that number as a uint64. */
uint64 Atoxll(const char * str);

#ifdef __cplusplus
} // end namespace muscle
#endif

#endif /* _MUSCLE_SUPPORT_H */
