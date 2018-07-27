/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */ 

#ifndef MuscleCloneable_h 
#define MuscleCloneable_h 

#include "support/MuscleSupport.h"

#ifdef MUSCLE_AVOID_CPLUSPLUS11
# include <typeinfo>
#else
# include <type_traits>
#endif

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

/** This macro declares a "virtual Cloneable * Clone() const" method that performs the standard/basic Clone() implementation:  Allocates a duplicate of this object on the heap, using the copy constructor, and returns it.  The macro allows us to avoid the tedious and error-prone re-entering of the same three lines of code for every Cloneable class.  (If there was a way to automate this using templates, I'd use that instead, but I haven't seen a reasonable way to do that yet) */
#define DECLARE_STANDARD_CLONE_METHOD(class_name) virtual Cloneable * Clone() const {Cloneable * r = newnothrow class_name(*this); if (r == NULL) WARN_OUT_OF_MEMORY; return r;}

#ifdef MUSCLE_AVOID_CPLUSPLUS11

/** 
  * Returns a heap-allocated clone of the passed-in object.
  * @param item an item to call Clone() on if possible
  * @returns a heap-allocated copy of (item) on success, or NULL on failure.
  * @note This is an old-style (pre-C++11) implementation of CloneObject() that 
  *       relies on dynamic_cast<> to determine if the argument inherits
  *       from Cloneable, and if it doesn't, this method will print an error
  *       to stdout and return NULL.  The C++11-aware version (below) is
  *       superior, in that it will do the right thing whether the passed-in
  *       argument derived from Cloneable or not.
  */
template<typename T> T * CloneObject(const T & item) 
{
   const Cloneable * c = dynamic_cast<const Cloneable *>(&item);
   if (c) return static_cast<T *>(c->Clone());
   else
   {
      printf("muscle::CloneObject:  Can't clone an object of type [%s] without C++11 support, since it doesn't derive from muscle::Cloneable!\n", typeid(item).name());
      return NULL;
   }
}

#else

/** 
  * Returns a heap-allocated clone of the passed-in object.
  * @param item an item to call Clone() on if possible, or to clone
  *             using the new operator and the object's copy-constructor otherwise.
  * @returns a heap-allocated copy of (item) on success, or NULL on failure.
  * @note this version is used (via SFINAE) for any type that inherits from Cloneable.
  */
template <typename T>
typename std::enable_if<std::is_base_of<Cloneable, T>::value, T*>::type
CloneObject(const T& item)
{
   return static_cast<T *>(item.Clone());
}

/** 
  * Returns a heap-allocated clone of the passed-in object.
  * @param item an item to call Clone() on if possible, or to clone
  *             using the new operator and the object's copy-constructor otherwise.
  * @returns a heap-allocated copy of (item) on success, or NULL on failure.
  * @note this version is used (via SFINAE) for any type that does NOT inherit from Cloneable.
  */
template <typename T>
typename std::enable_if<!std::is_base_of<Cloneable, T>::value, T*>::type
CloneObject(const T& item)
{
   T * newItem = newnothrow T(item);
   if (newItem) return newItem;
   else
   {
      WARN_OUT_OF_MEMORY;
      return NULL;
   }
}

#endif

} // end namespace muscle

#endif
