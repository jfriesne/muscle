/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDemandConstructedObject_h
#define MuscleDemandConstructedObject_h

#include "support/MuscleSupport.h"

#if defined(__MINGW32__) || defined(__MINGW64__)
# include <new>
#endif

namespace muscle {

/** This class wraps an object of the specified type, so that the wrapped object can be used as
 *  a member object but, without requiring its constructor to be called at the usual C++ construct-object time.
 *
 *  Instead, the wrapped object's constructor gets called at a time of the caller's choosing;
 *  typically when the wrapped object is first accessed.  This can be useful to avoid the 
 *  overhead of constructing an object that may or may not ever be actually used for anything, 
 *  while at the same time avoiding the overhead and uncertainty of a separate dynamic memory 
 *  allocation for the object.
 */
template <typename T> class DemandConstructedObject
{
public:
   /** Constructor.  */
   DemandConstructedObject() : _objPointer(NULL) {/* empty*/}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   DemandConstructedObject(const DemandConstructedObject<T> & rhs) : _objPointer(NULL) {if (rhs.IsObjectConstructed()) (void) EnsureObjectConstructed(rhs.GetObjectUnchecked());}

   /** Pseudo-Copy constructor. 
     * @param rhs the object to make this object a logical copy of
     */
   DemandConstructedObject(const T & rhs) : _objPointer(NULL) {(void) EnsureObjectConstructed(rhs);}

   /** Destructor.  Calls the destructor on our held object as well, if necessary. */
   ~DemandConstructedObject() {if (_objPointer) _objPointer->~T();}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   DemandConstructedObject & operator=(const DemandConstructedObject & rhs) 
   {
      if (rhs.IsObjectConstructed()) GetObject() = rhs.GetObjectUnchecked();
                                 else (void) EnsureObjectDestructed();
      return *this;
   }

   /** Templated Assignment operator, for convenience
     * @param rhs the data object to set our own held object equal to
     */
   DemandConstructedObject & operator=(const T & rhs) 
   {
      (void) GetObject() = rhs;
      return *this;
   }

   /** Equality operator.  Destructed objects are always considered equal.
     * Constructed objects are never considered equal to destructed objects.
     * Two constructed objects will be considered equal according to their == operator.
     * @param rhs the object to compare against
     */
   bool operator == (const DemandConstructedObject & rhs) const
   {
      bool amConstructed = IsObjectConstructed();
      if (amConstructed != rhs.IsObjectConstructed()) return false;
      return ((amConstructed == false)||(GetObjectUnchecked() == rhs.GetObjectUnchecked()));
   }

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
   bool operator != (const DemandConstructedObject & rhs) const {return !(*this==rhs);}

   /** Equality operator.  Returns true iff our held object is constructed and equal to (rhs)
     * @param rhs the object to compare against
     */
   bool operator == (const T & rhs) const {return ((IsObjectConstructed())&&(GetObjectUnchecked() == rhs));}

   /** Inequality operator.  Returns true iff our held object is not constructed or not equal to (rhs)
     * @param rhs the object to compare against
     */
   bool operator != (const T & rhs) const {return !(*this==rhs);}

   /** Returns a valid reference to our wrapped object.  EnsureObjectConstructed() will be called if necessary. */
   T & GetObject() {(void) EnsureObjectConstructed(); return *_objPointer;}

   /** Read-only version of GetObject.  EnsureObjectConstructed() will be called if necessary. */
   const T & GetObject() const {(void) EnsureObjectConstructed(); return *_objPointer;}

   /** Returns a reference to our wrapped object.  Note that this method DOES NOT
     * call EnsureObjectConstructed(), so if you call this method you MUST be sure the object
     * was already constructed, or you will get back a NULL reference.
     */
   T & GetObjectUnchecked() {return *_objPointer;}

   /** Read-only version of GetObjectUnchecked(). */
   const T & GetObjectUnchecked() const {return *_objPointer;}

   /** Ensures that our held object is constructed.  It's not typically necessary to call
     * this method manually, because GetObject() will call it for you when necessary.
     * If the object was already constructed, then this method is a no-op.
     * @returns true if this call constructed the object, or false if the object was already constructed.
     */
   bool EnsureObjectConstructed() const {if (_objPointer) return false; else {_objPointer = new (_objUnion._buf) T(); return true;}}

   /** Same as above, except if this call needs to construct the object, it does so using the object's
     * copy-constructor, and passed in (val) as the object to copy from.
     * @param val The object to copy from, if we need to construct our object.
     * @returns true if this call constructed the object, or false if the object was already constructed.
     */
   bool EnsureObjectConstructed(const T & val) const {if (_objPointer) return false; else {_objPointer = new (_objUnion._buf) T(val); return true;}}

   /** Calls our held object's destructor, if necessary.
     * If the object isn't currently valid, then this method is a no-op.
     * @returns true if this call destructed the object, or false if the object was already destructed.
     */
   bool EnsureObjectDestructed() const {if (_objPointer) {_objPointer->~T(); _objPointer = NULL; return true;} else return false;}

   /** Returns true iff the held object pointer is currently constructed. */
   bool IsObjectConstructed() const {return _objPointer != NULL;}

private:
   mutable T * _objPointer;

#ifndef DOXYGEN_SHOULD_IGNORE_THIS  // this is here so doxygen-coverage won't complain that I haven't documented this union -- but it's a private union so I don't need to
   mutable union {
      T * _junk;   // only here ensure object-friendly alignment
      uint8 _buf[sizeof(T)];
   } _objUnion;
#endif
};

} // end namespace muscle

#endif
