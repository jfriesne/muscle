/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleCountedObject_h
#define MuscleCountedObject_h

#include <typeinfo>   // So we can use typeid().name() in GetCounterTypeName()
#include "support/NotCopyable.h"
#include "system/AtomicCounter.h"
#include "util/Hashtable.h"

namespace muscle {

#ifdef MUSCLE_ENABLE_OBJECT_COUNTING

/** This base class is used to construct a linked-list of ObjectCounter objects so that we can iterate over them and print them out */
class ObjectCounterBase : private NotCopyable
{
public:
   /** Constructor
     * @param objectCounterTypeName a human-readable string describing the type we want to count.
     * @param sizeofObject Pass in sizeof(Object) for the types of objects we are to be counting.
     */
   ObjectCounterBase(const char * objectCounterTypeName, uint32 sizeofObject);

   /** To be implemented by ObjectCounter subclass to return a human-readable name indicating the type that is being counted */
   const char * GetCounterTypeName() const {return _objectCounterTypeName;}

   /** Returns the number of objects of our type that are currently allocated. */
   uint32 GetCount() const {return _counter.GetCount();}

   /** Returns sizeof(Object) for the object-type we are counting */
   uint32 GetSizeofObject() const {return _sizeofObject;}

   /** Returns the previous counter in our global linked-list of ObjectCounters. */
   const ObjectCounterBase * GetPreviousCounter() const {return _prevCounter;}

   /** Returns the next counter in our global linked-list of ObjectCounters. */
   const ObjectCounterBase * GetNextCounter() const {return _nextCounter;}

   /** Increments our internal count */
   void IncrementCounter() {_counter.AtomicIncrement();}

   /** Decrements our internal count.  Returns true iff our internal count has reached zero. */
   bool DecrementCounter() {return _counter.AtomicDecrement();}

protected:
   /** Constructor.  Only our ObjectCounter subclass is allowed to construct us! */
   ObjectCounterBase();

   /** Destructor. */
   ~ObjectCounterBase();

private:
   void PrependObjectCounterBaseToGlobalCountersList();
   void RemoveObjectCounterBaseFromGlobalCountersList();

   const char * _objectCounterTypeName;
   const uint32 _sizeofObject;
   ObjectCounterBase * _prevCounter;
   ObjectCounterBase * _nextCounter;
   AtomicCounter _counter;
};

/** This class is used by the CountedObject class to count objects.  You shouldn't ever need to instantiate an object of this class directly. */
template <class ObjectType> class ObjectCounter : public ObjectCounterBase
{
public:
   /** Default constructor */
   ObjectCounter() : ObjectCounterBase(typeid(ObjectType).name(), (uint32)sizeof(ObjectType)) {/* empty */}
};

#endif

/** This is a class that other classes can derive from, or keep as a
  * private member variable, if it is desired to keep track of the number 
  * of objects of that other class that are currently allocated.  Note that 
  * this class will compile down to a no-op unless -DMUSCLE_ENABLE_OBJECT_COUNTING 
  * is present.  Otherwise, you can call PrintCountedObjectInfo() at any time 
  * to get a report of current object allocation counts by type.
  */
template <class ObjectType> class CountedObject
{
public:
   /** Default Constructor.  */      
   CountedObject() 
   {
#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
      GetGlobalObjectForType< ObjectCounter<ObjectType> >().IncrementCounter();
#endif
   }

   /** Copy Constructor.  */      
   CountedObject(const CountedObject<ObjectType> & /*rhs*/) 
   {
#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
      GetGlobalObjectForType< ObjectCounter<ObjectType> >().IncrementCounter();
#endif
   }

   /** Destructor (deliberately not virtual, to avoid a vtable-pointer size-penalty) */
   ~CountedObject() 
   {
#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
      GetGlobalObjectForType< ObjectCounter<ObjectType> >().DecrementCounter();
#endif
   }

   /** Assignment operator -- implemented only to avoid compiler warnings */
   CountedObject & operator =(const CountedObject<ObjectType> & /*rhs*/) {return *this;}
};

/** For debugging.  On success, populates (results) with type names and their associated object counts,
  * and sizeof-object-in-bytes values, respectively.
  * @param results a Hashtable to populate.  Keys in this table are human-readable(ish) strings identifying the class,
  *                and values are bit-chords with the lower 32 bits being the number of objects of that class that are currently in existence,
  *                and the upper 32 bits being sizeof() an object of that class.
  * @returns B_NO_ERROR on success, or B_LOCK_FAILED, or B_OUT_OF_MEMORY, or B_UNIMPLEMENTED (if -DMUSCLE_TRACK_OBJECT_COUNTS wasn't defined).
  */
status_t GetCountedObjectInfo(Hashtable<const char *, uint64> & results);

/** Convenience function.  Calls GetCountedObjectInfo() and pretty-prints the results to stdout. */
void PrintCountedObjectInfo();

#if defined(MUSCLE_ENABLE_OBJECT_COUNTING)
# define DECLARE_COUNTED_OBJECT(className) CountedObject<className> _declaredCountedObject /**< Macro to declare CountedObject<className> as a member variable, iff MUSCLE_ENABLE_OBJECT_COUNTING is defined */
#else
  // Note:  the only reason for this typedef is to serve as filler, to avoid "extra semicolon" warnings when compiling in clang++ with -Wpedantic
# define DECLARE_COUNTED_OBJECT(className) typedef int unused_type /**< Macro to declare CountedObject<className> as a member variable, iff MUSCLE_ENABLE_OBJECT_COUNTING is defined */
#endif

} // end namespace muscle

#endif
