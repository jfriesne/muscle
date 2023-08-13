/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleTamperEvidentValue_h
#define MuscleTamperEvidentValue_h

#include "support/MuscleSupport.h"

namespace muscle {

/** A simple templated class that holds a single value of the template-specified type.
  * The only difference between using a TamperEvidentValue<T> and just using the T object directly
  * is that the TamperEvidentValue class will automatically set a flag whenever the stored
  * value is set (via SetValue() or assignment operator), so that you can tell later on if
  * anyone has explicitly set this value after it was constructed.
  * @tparam T the type of value we should hold and flag changes to.
  */
template <typename T> class MUSCLE_NODISCARD TamperEvidentValue
{
public:
   /** Default constructor */
   MUSCLE_CONSTEXPR TamperEvidentValue() : _value(), _wasExplicitlySet(false) {/* empty */}

   /** Explicit constructor -- sets our value to the specified value, but doesn't set the value-was-explicitly-set flag.
     * @param val the value to initially set this object to.
     */
   MUSCLE_CONSTEXPR TamperEvidentValue(const T & val) : _value(val), _wasExplicitlySet(false) {/* empty */}

   /** Copy constructor.  Copies both the value and the flag-state from the passed-in TamperEvidentValue object.
     * @param rhs the object to make this object a duplicate of
     */
   MUSCLE_CONSTEXPR TamperEvidentValue(const TamperEvidentValue & rhs) : _value(rhs._value), _wasExplicitlySet(rhs._wasExplicitlySet) {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &)
     * @note our HasValueBeenSet() flag will be set to true by this operation
     */
   TamperEvidentValue & operator =(const TamperEvidentValue & rhs) {SetValue(rhs.GetValue()); return *this;}

   /** Assignment operator.
     * @param rhs the value to set this object to hold.
     * @note our HasValueBeenSet() flag will be set to true by this operation
     */
   TamperEvidentValue & operator =(const T & rhs) {SetValue(rhs); return *this;}

   /** Sets a new value, and also sets our HasValueBeenSet() flag to true.
     * @param newVal the value to explicitely-set this object to
     */
   void SetValue(const T & newVal) {_value = newVal; _wasExplicitlySet = true;}

   /** Returns our current value */
   MUSCLE_NODISCARD const T & GetValue() const {return _value;}

   /** Conversion operator, for convenience */
   MUSCLE_NODISCARD operator T() const {return _value;}

   /** Returns true iff SetValue() was called after this object was constructed. */
   MUSCLE_NODISCARD bool HasValueBeenSet() const {return _wasExplicitlySet;}

   /** Call this if you want to set the HasValueBeenSet() flag back to false again. */
   void ClearValueWasSetFlag() {_wasExplicitlySet = false;}

private:
   T _value;
   bool _wasExplicitlySet;
};

} // end namespace muscle

#endif
