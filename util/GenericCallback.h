/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#ifndef MuscleGenericCallback_h
#define MuscleGenericCallback_h

#include "util/RefCount.h"

namespace muscle {

/** Interface class representing a functor object whose GenericCallback() method can be called.  
  * The exact semantics of what the call does are not defined here; it can be used for different
  * purposes in different contexts.
  */
class GenericCallback : public RefCountable
{
public:
   /** Constructor */
   GenericCallback() {/* empty */}

   /** Virtual destructor, to keep C++ honest */
   virtual ~GenericCallback() {/* empty */}

   /** This method will be called by the caller.
     * @param arg An argument, the semantics of which are defined by the use case.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.  The semantics of what these mean are defined by the use case.
     */
   virtual status_t Callback(void * arg) = 0;
};
DECLARE_REFTYPES(GenericCallback);

/** Convenience subclass that lets you specify a function pointer for the Callback() method to call. */
class FunctionCallback : public GenericCallback
{
public:
   /** Signature of the first function type that we know how to call:  "void MyFunctionName()" */
   typedef void (*FunctionCallbackTypeA)();

   /** Constructor.
     * @param f The function to call in our Callback() method.  This function should be of type "void MyFunc()"
     */
   FunctionCallback(FunctionCallbackTypeA f) : _funcA(f), _funcB(NULL), _arg(NULL) {/* empty */}

   /** Signature of the second function type that we know how to call:  "status_t MyFunctionName(void *)" */
   typedef status_t (*FunctionCallbackTypeB)(void *);

   /** Constructor.
     * @param f The function to call in our Callback() method.  This function should be of type "status_t MyFunc(void *)"
     * @param arg The argument to pass to the function.
     */
   FunctionCallback(FunctionCallbackTypeB f, void * arg) : _funcA(NULL), _funcB(f), _arg(arg) {/* empty */}

   /** Calls through to the function specified in the constructor. */
   virtual status_t Callback(void *) 
   {
      if (_funcA) {_funcA(); return B_NO_ERROR;}
      return _funcB ? _funcB(_arg) : B_ERROR;
   }

private:
   FunctionCallbackTypeA _funcA;
   FunctionCallbackTypeB _funcB;
   void * _arg;
};

}; // end namespace muscle

#endif
