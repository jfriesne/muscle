/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleCountedObject_h
#define MuscleCountedObject_h

#include <typeinfo>   // So we can use typeid().name() in GetCounterTypeName()
#include "system/AtomicCounter.h"
#include "util/Hashtable.h"

namespace muscle {

#ifndef MUSCLE_AVOID_OBJECT_COUNTING

/** This base class is used to construct a linked-list of ObjectCounter objects so that we can iterate over them and print them out */
class ObjectCounterBase
{
public:
   /** To be implemented by ObjectCounter subclass to return a human-readable name indicating the type that is being counted */
   virtual const char * GetCounterTypeName() const = 0;

   /** Implemented by subclass to return the number of objects of our type that are currently allocated. */
   uint32 GetCount() const {return _counter.GetCount();}

   /** Returns the previous counter in our global list of ObjectCounters. */
   const ObjectCounterBase * GetPreviousCounter() const {return _prevCounter;}

   /** Returns the next counter in our global list of ObjectCounters. */
   const ObjectCounterBase * GetNextCounter() const {return _nextCounter;}

   /** Increments our internal count */
   void IncrementCounter() {_counter.AtomicIncrement();}

   /** Decrements our internal count.  Returns true iff our internal count has reached zero. */
   bool DecrementCounter() {return _counter.AtomicDecrement();}

protected:
   /** Constructor.  Only our ObjectCounter subclass is allowed to construct us! */
   ObjectCounterBase();

   /** Destructor. */
   virtual ~ObjectCounterBase();

private:
   ObjectCounterBase(const ObjectCounterBase &);  // private and unimplemented

   ObjectCounterBase * _prevCounter;
   ObjectCounterBase * _nextCounter;
   AtomicCounter _counter;
};

/** This class is used by the CountedObject class to count objects.  You shouldn't ever need to instantiate an object of this class directly. */
template <class ObjectType> class ObjectCounter : public ObjectCounterBase
{
public:
   /** Default constructor */
   ObjectCounter() {/* empty */}

   /** Destructor */
   virtual ~ObjectCounter() {/* empty */}

   /** Implemented to return a human-readable string describing ObjectType's type */
   virtual const char * GetCounterTypeName() const {return typeid(ObjectType).name();}
};

#endif

/** This class is a superclass that other classes can derive from if it is
  * desired to keep track of the number of objects of the derived class
  * that are currently allocated.  Note that this class will compile down
  * to a no-op if -DMUSCLE_AVOID_OBJECT_COUNTING is present.  Otherwise, you can
  * call PrintCountedObjectInfo() at any time to get a report of current object
  * allocation counts by type.
  */
template <class ObjectType> class CountedObject
{
public:
   /** Default Constructor.  */      
   CountedObject() 
   {
#ifndef MUSCLE_AVOID_OBJECT_COUNTING
      GetGlobalObjectForType< ObjectCounter<ObjectType> >().IncrementCounter();
#endif
   }

   /** Copy Constructor.  */      
   CountedObject(const CountedObject<ObjectType> & /*rhs*/) 
   {
#ifndef MUSCLE_AVOID_OBJECT_COUNTING
      GetGlobalObjectForType< ObjectCounter<ObjectType> >().IncrementCounter();
#endif
   }

   /** Destructor (deliberately not virtual, to avoid a vtable-pointer size-penalty) */
   ~CountedObject() 
   {
#ifndef MUSCLE_AVOID_OBJECT_COUNTING
      GetGlobalObjectForType< ObjectCounter<ObjectType> >().DecrementCounter();
#endif
   }
};

/** For debugging.  On success, populates (results) with type names and their associated object counts,
  * respectively.
  * @param results a Hashtable to populate.
  * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory, or -DMUSCLE_TRACK_OBJECT_COUNTS wasn't defined)
  */
status_t GetCountedObjectInfo(Hashtable<const char *, uint32> & results);

/** Convenience function.  Calls GetCountedObjectInfo() and pretty-prints the results to stdout. */
void PrintCountedObjectInfo();

}; // end namespace muscle

#endif
