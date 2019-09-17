/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDoxyTemplate_h
#define MuscleDoxyTemplate_h

// deliberately *not* putting this class inside the muscle namespace, for simplicity's sake (since it's never compiled anyway)

#ifdef DOXYGEN_SHOULD_IGNORE_THIS  // yes, the #ifdef (rather than #ifndef) is deliberate here

/** This class isn't meant to be compiled the C++ compiler; rather, 
  * we use it as a repository of standard Doxygen comments for ubiquitous class-members.
  * That way we don't have to repeat the same boilerplate comments over and over 
  * for every class in the distribution
  */
class DoxyTemplate MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor */
   DoxyTemplate();

   /** Copy constructor
     * @param rhs the object that this object is to become a duplicate of
     */
   DoxyTemplate(const DoxyTemplate & rhs);

   /** Move constructor (C++11 only)
     * @param rhs the object whose state we should steal away to use as our own
     */
   DoxyTemplate(DoxyTemplate && rhs);

   /** Destructor */
   ~DoxyTemplate();
 
   /** Assignment operator.  Sets this object to be a duplicate of (rhs)
     * @param rhs the object that this object should become a duplicate of
     * @returns a reference to this object
     */
   DoxyTemplate & operator =(const DoxyTemplate & rhs);

   /** Move-Assignment operator (C++11 only).  Sets this object to be a duplicate of 
     * (rhs) by stealing the state of (rhs) away so that this object can use it.
     * @param rhs the object that this object can plunder to assume the same state that (rhs) had
     * @returns a reference to this object
     */
   DoxyTemplate & operator =(DoxyTemplate && rhs);

   /** Equality operator.
     * @param rhs the object to compare this object against
     * @returns true iff this object is equal to (rhs)
     */
   bool operator ==(const DoxyTemplate & rhs) const;

   /** Inequality operator.
     * @param rhs the object to compare this object against
     * @returns true iff this object is not equal to (rhs)
     */
   bool operator !=(const DoxyTemplate & rhs) const;

   /** Less-than operator.
     * @param rhs the object to compare this object against
     * @returns true iff this object is less than (rhs)
     */
   bool operator < (const DoxyTemplate &rhs) const;

   /** Greater-than operator.
     * @param rhs the object to compare this object against
     * @returns true iff this object is greater than (rhs)
     */
   bool operator > (const DoxyTemplate &rhs) const;

   /** Less-than-or-equal-to operator.
     * @param rhs the object to compare this object against
     * @returns true iff this object is not greater than (rhs)
     */
   bool operator <=(const DoxyTemplate &rhs) const;

   /** Greater-than-or-equal-to operator.
     * @param rhs the object to compare this object against
     * @returns true iff this object is not less than (rhs)
     */
   bool operator >=(const DoxyTemplate &rhs) const;

   /** In-place union operator.  Updates this object to be the union
     * of itself and the passed-in object.
     * @param rhs the object to unify with this one
     * @returns a reference to this object.
     */
   DoxyTemplate & operator |=(const DoxyTemplate & rhs);

   /** In-place intersection operator.  Updates this object to be the intersection
     * of itself and the passed-in object.
     * @param rhs the object to intersect with this one
     * @returns a reference to this object.
     */
   DoxyTemplate & operator &=(const DoxyTemplate & rhs);

   /** In-place exclusive-or operator.  Updates this object to be the exclusive-or
     * of itself and the passed-in object.
     * @param rhs the object to exclusive-or with this one
     * @returns a reference to this object.
     */
   DoxyTemplate & operator ^=(const DoxyTemplate & rhs);

   /** Union operator.
     * @param rhs the object to compute the union of this object with
     * @returns an object that is the union of this object and the passed-in object.
     */
   DoxyTemplate & operator |(const DoxyTemplate & rhs) const;

   /** Intersection operator.
     * @param rhs the object to compute the intersection of this object with
     * @returns an object that is the intersection of this object and the passed-in object.
     */
   DoxyTemplate & operator &(const DoxyTemplate & rhs) const;

   /** Exclusive-or operator.
     * @param rhs the object to compute the exclusive-or of this object with
     * @returns an object that is the exclusive-or of this object and the passed-in object.
     */
   DoxyTemplate & operator ^(const DoxyTemplate & rhs) const;

   /** Bitwise negation operator.
     * @returns an object that is the bitwise negation of this object.
     */
   DoxyTemplate & operator ~() const;

   /** Returns true iff the FlattenedSize() method of this class will always return the same value for all possible objects of this class. */
   bool IsFixedSize() const;

   /** Returns the type code value (e.g. B_STRING_TYPE or whatever) that represents this class */
   uint32 TypeCode() const;

   /** Returns true iff the supplied type code value represents a type we know how to Unflatten() from.
     * @param tc the type-code value to check for appropriateness.
     */
   bool AllowsTypeCode(uint32 tc) const;

   /** Returns the number of bytes that Flatten() will write out to its supplied buffer, if called now */
   uint32 FlattenedSize() const;

   /** Writes this object's state out to the supplied memory buffer.
    *  @param buffer Must points to an array of at least (FlattenedSize()) bytes.
    */
   void Flatten(uint8 * buffer) const;

   /** Restores this object's state from the data contained in the supplied memory buffer.
    *  @param buffer points to the raw data we should read in this object's state from.
    *  @param size The number of bytes (buffer) points at
    *  @return B_NO_ERROR on success, B_BAD_DATA if (size) was too small, or B_BAD_DATA if data was inappropriate, or etc.
    */
   status_t Unflatten(const uint8 * buffer, uint32 size);

   /** Returns a 32-bit hash code calculated based on the current state of this object.
     * @note Implementing this method (and operator==()) is what allows a non-POD class to be used as a key in a Hashtable.
     */
   uint32 HashCode() const;

   /** Returns a 64-bit hash code calculated based on the current state of this object. */
   uint64 HashCode64() const;

   /** Returns a 32-bit checksum calculated based on the current state of this object. */
   uint32 CalculateChecksum() const;

   /** Returns a human-readable string representing the current state of this object, for debugging purposes. */
   String ToString() const;

   /** Prints a human-readable representation of this object's state to stdout, for debugging purposes. */
   void PrintToStream() const;

   /** Writes this object's state out to the given Message object.
    *  @param archive the Message to write our state into.
    *  @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t SaveToArchive(Message & archive) const;

   /** Sets this object's state from the contents of the given Message object.
    *  @param archive The Message to restore our state (typically a Message that was previously populated by SaveToArchive())
    *  @returns B_NO_ERROR on success, or B_BAD_DATA (or etc) on failure.
    */
   status_t SetFromArchive(const Message & archive);
};

#endif

#endif 
