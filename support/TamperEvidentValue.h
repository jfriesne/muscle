/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleTamperEvidentValue_h
#define MuscleTamperEvidentValue_h

#include "support/MuscleSupport.h"

namespace muscle {

/** A simple templated class that holds a single value of the template-specified type.
  * The only difference between using a TamperEvidentValue<T> and just using the T object directly
  * is that the TamperEvidentValue class will automatically set a flag whenever the stored
  * value is set (via SetValue() or assignment operator), so that you can tell later on if
  * anyone has explicitly set this value after it was constructed.
  */
template <typename T> class TamperEvidentValue
{
public:
   /** Default constructor */
   TamperEvidentValue() : _value(), _wasExplicitlySet(false) {/* empty */}

   /** Explicit constructor.  */
   TamperEvidentValue(const T & val) : _value(val), _wasExplicitlySet(false) {/* empty */}

   /** Copy constructor.  Copies both the value and the flag-state from the passed-in TamperEvidentValue object. */
   TamperEvidentValue(const TamperEvidentValue & copyMe) : _value(copyMe._value), _wasExplicitlySet(copyMe._wasExplicitlySet) {/* empty */}

   /** Destructor */
   ~TamperEvidentValue() {/* empty */}

   /** Assignment operator. */
   TamperEvidentValue & operator =(const TamperEvidentValue & rhs) {SetValue(rhs.GetValue()); return *this;}

   /** Assignment operator. */
   TamperEvidentValue & operator =(const T & rhs) {SetValue(rhs); return *this;}

   /** Sets a new value, and also sets our HasValueBeenSet() flag to true. */
   void SetValue(const T & newVal) {_value = newVal; _wasExplicitlySet = true;}

   /** Returns our current value */
   const T & GetValue() const {return _value;}

   /** Conversion operator, for convenience */
   operator T() const {return _value;}

   /** Returns true iff SetValue() was called after this object was constructed. */
   bool HasValueBeenSet() const {return _wasExplicitlySet;}

   /** Call this if you want to set the HasValueBeenSet() flag back to false again. */
   void ClearValueWasSetFlag() {_wasExplicitlySet = false;}

private:
   T _value;
   bool _wasExplicitlySet;
};

}; // end namespace muscle

#endif
