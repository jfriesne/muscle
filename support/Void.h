/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleVoid_h
#define MuscleVoid_h

#include "support/MuscleSupport.h"

namespace muscle {

/** This is a class that literally represents no data.  It is primarily meant to be 
  * used as a dummy/placeholder type in Hashtables that need to contain only keys 
  * and don't need to contain any values.
  */
class Void MUSCLE_FINAL_CLASS
{
public:
   /** Default ctor */
   Void() {/* empty */}

   /** Always returns false -- all Voids are created equal and therefore one can't be less than another. */
   bool operator <(const Void &) const {return false;}

   /** Always returns true -- all Voids are created equal */
   bool operator ==(const Void &) const {return true;}

   /** Always returns false -- all Voids are created equal */
   bool operator !=(const Void &) const {return false;}
};

} // end namespace muscle

#endif
