/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleBatchOperator_h
#define MuscleBatchOperator_h

#include "util/NestCount.h"

namespace muscle {

/** This macro is the best way to declare a BatchGuard object */
#ifdef _MSC_VER
#define DECLARE_BATCHGUARD(bo, ...) const BatchOperatorBase::BatchGuard & MUSCLE_UNIQUE_NAME = (bo).GetBatchGuard(__VA_ARGS__); (void) MUSCLE_UNIQUE_NAME
#else
# define DECLARE_BATCHGUARD(bo, args...) const BatchOperatorBase::BatchGuard & MUSCLE_UNIQUE_NAME = (bo).GetBatchGuard(args); (void) MUSCLE_UNIQUE_NAME
#endif

/** This class includes the common functionality of a BatchOperator that is the same across all template
  * instantiations of the BatchOperator family.  It should not be used directly by user code.
  */
class BatchOperatorBase
{
public:
   /** This class is used by the DECLARE_BATCHGUARD() macro.  Do not access it directly. */
   class BatchGuard
   {
   public:
      /** Default constructor. */
      BatchGuard() {/* empty */}
   }; 

   /** Read-only access to our internal NestCount object, in case you are interested in querying its state. */
   const NestCount & GetNestCount() const {return _count;}

protected:
   /** This constructor is available only to our concrete BatchOperator subclasses. */
   BatchOperatorBase() {/* empty */}

   /** This object keeps track of how many layers into the batch's call tree we currently are. */
   NestCount _count;
};

/** This class is meant to represent an object that can do a series of operations more
  * efficiently if it knows when the series is starting and ending.  To use it, you would
  * subclass this class, implement its BatchBegins() and BatchEnds() methods, and then place
  * BatchGuard objects on the stack in the calling code, at the top of any routine
  * that may be doing one or more operations in sequence.  The benefit is that your BatchBegins()
  * and BatchEnd() routines would then be automatically called at the proper times, and 
  * nesting/recursion is guaranteed to be handled correctly.
  *
  * This is a slightly more complex implementation of BatchOperator, in that it allows
  * you to specify a BatchArgs argument that is associated with the batch.  The BatchArgs
  * argument may be of any type that you care to specify, and can be used either to
  * convey information about that batch (e.g. the batch's name, for undo purposes), or
  * to differentiate different types of batch operation (if you want to use batching
  * of different kinds within the same object), or both.
  */
template <typename BatchArgs = void> class BatchOperator : public BatchOperatorBase
{
public:
   /** Default constructor. */
   BatchOperator() {/* empty */}

   /** Destructor */
   virtual ~BatchOperator() {/* empty */}

   /** You can explicitly call this at the beginning of a batch, although it's safer to instantiate a BatchGuard object
     * to call this method for you; that way its destructor will be sure to call the matching EndOperationBatch() at the proper time.
     * @param args The arguments to pass to BatchBegins(), if we are the outermost level of batch nesting.
     * @returns true iff BatchBegins() was called, or false if it wasn't (because we were already in a batch)
     */
   inline bool BeginOperationBatch(const BatchArgs & args = BatchArgs()) {if (_count.Increment()) {BatchBegins(args); return true;} else return false;}

   /** You should explicitly call this at the end of any batch that you explicitly called BeginOperationBatch() for.
     * Note that it's safer to instantiate a BatchGuard object so that its destructor will call this method for you.
     * @param args The arguments to pass to BatchEnds(), if we are the outermost level of batch nesting.
     * @returns true iff BatchEnds() was called, or false if it wasn't (because we were already in a batch)
     */
   inline bool EndOperationBatch(const BatchArgs & args = BatchArgs()) 
   {
      bool ret = false;
      if (_count.IsOutermost()) {BatchEnds(args); ret = true;}  // note that BatchEnds() is called while _count is still non-zero!
      (void) _count.Decrement();
      return ret;
   }

protected:
   /** Called by BeginOperationBatch(), when the outermost level of a batch begins.
     * You should implement it to do any setup steps that are required at the beginning of a series of operations.
     * @param args A user-defined object representing arguments for setting up the batch.
     */
   virtual void BatchBegins(const BatchArgs & args) = 0;

   /** Called by EndOperationBatch(), when the outermost level of a batch ends.
     * You should implement it to do any final steps that are required at the end of a series of operations.
     * @param args A user-defined object representing arguments for ending the batch.  This will be the same
     *             object that was previously passed to BatchBegins().
     */
   virtual void BatchEnds(const BatchArgs & args) = 0;

private:
   class BatchGuardImp : public BatchGuard
   {
   public:
      BatchGuardImp(BatchOperator<BatchArgs> & bop, const BatchArgs & args) : _bop(bop), _args(args) {(void) _bop.BeginOperationBatch(_args);}
      BatchGuardImp(const BatchGuardImp & rhs) : BatchGuard(), _bop(rhs._bop), _args(rhs._args) {(void) _bop.BeginOperationBatch(_args);}
      ~BatchGuardImp() {(void) _bop.EndOperationBatch(_args);}
      
   private:
      BatchOperator<BatchArgs> & _bop;
      BatchArgs _args;
   };

public:
   /** Returns a BatchGuard object that will keep this BatchOperator in a batch for as long as it exits.
     * This method should only be called indirectly, via the DECLARE_BATCHGUARD(ba) macro.
     * @param ba The BatchArgs object to pass to BeginOperationBatch().
     */
   BatchGuardImp GetBatchGuard(const BatchArgs & ba = BatchArgs()) {return BatchGuardImp(*this, ba);}
};

/** This class is meant to represent an object that can do a series of operations more
  * efficiently if it knows when the series is starting and ending.  To use it, you would
  * subclass this class, implement its BatchBegins() and BatchEnds() methods, and then place
  * BatchGuard objects on the stack in the calling code, at the top of any routine
  * that may be doing one or more operations in sequence.  The benefit is that your BatchBegins()
  * and BatchEnd() routines would then be automatically called at the proper times, and 
  * nesting/recursion is guaranteed to be handled correctly.
  *
  * This is a simple implementation of BatchOperator that doesn't use any batch arguments.
  */
template <> class BatchOperator<void> : public BatchOperatorBase
{
public:
   /** Default constructor. */
   BatchOperator() {/* empty */}

   /** Destructor. */
   virtual ~BatchOperator() {/* empty */}

   /** You can explicitly call this at the beginning of a batch, although it's safer to instantiate a BatchGuard object
     * to call this method for you; that way its destructor will be sure to call the matching EndOperationBatch() at the proper time.
     * @returns true iff BatchBegins() was called, or false if it wasn't (because we were already in a batch)
     */
   inline bool BeginOperationBatch() {if (_count.Increment()) {BatchBegins(); return true;} else return false;}

   /** You should explicitly call this at the end of any batch that you explicitly called BeginOperationBatch() for.
     * Note that it's safer to instantiate a BatchGuard object so that its destructor will call this method for you.
     * @returns true iff BatchEnds() was called, or false if it wasn't (because the batch was nested and we are therefore still in the batch)
     */
   inline bool EndOperationBatch()
   {
      bool ret = false;
      if (_count.IsOutermost()) {BatchEnds(); ret = true;}  // note that BatchEnds() is called while _count is still non-zero!
      (void) _count.Decrement();
      return ret;
   }

protected:
   /** Called by BeginOperationBatch(), when the outermost level of a batch begins.
     * You should implement it to do any setup steps that are required at the beginning of a series of operations.
     */
   virtual void BatchBegins() = 0;

   /** Called by EndOperationBatch(), when the outermost level of a batch ends.
     * You should implement it to do any final steps that are required at the end of a series of operations.
     */
   virtual void BatchEnds() = 0;

private:
   class BatchGuardImp : public BatchGuard
   {
   public:
      BatchGuardImp(BatchOperator & bop) : _bop(bop) {(void) _bop.BeginOperationBatch();}
      BatchGuardImp(const BatchGuardImp & rhs) : BatchGuard(), _bop(rhs._bop) {(void) _bop.BeginOperationBatch();}
      ~BatchGuardImp() {(void) _bop.EndOperationBatch();}
      
   private:
      BatchOperator<void> & _bop;
   };

public:
   /** Returns a BatchGuard object that will keep this BatchOperator in a batch for as long as it exits.
     * This method should only be called indirectly, via the DECLARE_BATCHGUARD() macro.
     */
   BatchGuardImp GetBatchGuard() {return BatchGuardImp(*this);}
};

}; // end namespace muscle

#endif
