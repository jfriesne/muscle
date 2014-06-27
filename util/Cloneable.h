/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */ 

#ifndef MuscleCloneable_h 
#define MuscleCloneable_h 

#include "support/MuscleSupport.h"

namespace muscle {

/** An interface that can be inherited by any class that wants to provide a Clone()
  * method that will return a copy of itself.
  */
class Cloneable
{
public:
   /** Default constructor.  */
   Cloneable() {/* empty */}

   /** Virtual destructor, to keep C++ honest.  Don't remove this unless you like crashing */
   virtual ~Cloneable() {/* empty */}

   /** Should be implemented by the inheriting concrete class to return a freshly allocated copy of itself. */
   virtual Cloneable * Clone() const = 0;
};
#define DECLARE_STANDARD_CLONE_METHOD(class_name) virtual Cloneable * Clone() const {Cloneable * r = newnothrow class_name(*this); if (r == NULL) WARN_OUT_OF_MEMORY; return r;}

/** A preferred version of CloneObject() that uses the argument's built-in Clone() method */
inline Cloneable * CloneObject(const Cloneable & item) {return item.Clone();}

#if DISABLED_FOR_NOW_SINCE_THE_COMPILER_SOMETIMES_USES_IT_WHEN_IT_SHOULDNT_JAF
/** A fallback version of CloneObject() for concrete types that don't support the Cloneable interface */
template<typename Item> inline Item * CloneObject(const Item & item)
{
   Item * c = newnothrow Item(item);
   if (c) *c = item;
     else WARN_OUT_OF_MEMORY;
   return c;
}
#endif

}; // end namespace muscle

#endif
