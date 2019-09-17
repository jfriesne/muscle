/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

/******************************************************************************
/
/   File:      PseudoFlattenable.h
/
/   Description:    no-op/empty/template-compatible version of the Flattenable interface
/
******************************************************************************/

#ifndef MusclePseudoFlattenable_h
#define MusclePseudoFlattenable_h

#include "support/MuscleSupport.h"

namespace muscle {

class Flattenable;

/** This class is here to support lightweight subclasses that want to have a Flattenable-like 
  * API (Flatten(), Unflatten(), etc) without incurring the one-word-per-object memory 
  * overhead caused by the presence of virtual methods.  To use this class, subclass your
  * class from this one and declare Flatten(), Unflatten(), FlattenedSize(), etc methods
  * in your class, but don't make them virtual.  That will be enough to allow you to
  * use Message::AddFlat(), Message::FindFlat(), etc on your objects, with no extra
  * memory overhead.  See the MUSCLE Point and Rect classes for examples of this technique.
  */ 
class PseudoFlattenable 
{
public:
   /**
    * Dummy implemention of CopyFrom().  It's here only so that Message::FindFlat() will
    * compile when called with a PseudoFlattenable object as an argument.
    * @param copyFrom This parameter is ignored.   
    * @returns B_UNIMPLEMENTED always, because given that this object is not a Flattenable object,
    *          it's assumed that it can't receive the state of a Flattenable object either.
    *          (but if that's not the case for your class, your subclass can implement its
    *           own CopyFrom() method to taste)
    */
   status_t CopyFrom(const Flattenable & copyFrom)
   {
#if !defined(_MSC_VER) || (_MSC_VER >= 1910)  // avoids error C2027: use of undefined type 'muscle::Flattenable'
      (void) copyFrom;
#endif
      return B_UNIMPLEMENTED;
   }
};

} // end namespace muscle

#endif /* MusclePseudoFlattenable_h */

