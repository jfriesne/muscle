/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

/******************************************************************************
/
/   File:      NotCopyable.h
/
/   Description:    Base class for classes we want to make sure are not copied.
/
******************************************************************************/

#ifndef MuscleNotCopyable_h
#define MuscleNotCopyable_h

#include "support/MuscleSupport.h"

namespace muscle {

/** This class can be inherited from as a quick and expressive way
 *  to mark an object as non-copyable.  The compiler will then throw
 *  a compile error if the programmer tries to use its copy-constructor
 *  or its assignment operator.
 */
class NotCopyable 
{
public:
   /** Default constructor, does nothing. */
   NotCopyable() {/* empty */}

private:
#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** Copy constructor, deliberately made non-callable.  Trying to use it will cause a compile-time error. */
   NotCopyable(const NotCopyable &) = delete;

   /** Assignment operator, deliberately made non-callable.  Trying to use it will cause a compile-time error. */
   NotCopyable & operator = (const NotCopyable &) = delete;
#else
   /** Copy constructor, deliberately made non-callable.  Trying to use it will cause a compile-time error. */
   NotCopyable(const NotCopyable &);                // deliberately private and unimplemented

   /** Assignment operator, deliberately made non-callable.  Trying to use it will cause a compile-time error. */
   NotCopyable & operator = (const NotCopyable &);  // deliberately private and unimplemented
#endif
};

} // end namespace muscle

#endif /* MuscleNotCopyable_h */

