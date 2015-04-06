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
   NotCopyable() {/* empty */}

private:
#ifdef MUSCLE_USE_CPLUSPLUS11
   NotCopyable(const NotCopyable &) = delete;
   NotCopyable & operator = (const NotCopyable &) = delete;
#else
   NotCopyable(const NotCopyable &);                // deliberately private and unimplemented
   NotCopyable & operator = (const NotCopyable &);  // deliberately private and unimplemented
#endif
};

}; // end namespace muscle

#endif /* MuscleNotCopyable_h */

