/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleObjectPool_h
#define MuscleObjectPool_h

#include <typeinfo>   // So we can use typeid().name() in the assertion failures
#include "system/Mutex.h"

namespace muscle {

// Uncomment this #define to disable object pools (i.e. turn them into
// simple passthroughs to the new/delete operators).  
// This is helpful if you are trying to track down memory leaks.
//#define DISABLE_OBJECT_POOLING 1

#ifndef DEFAULT_MUSCLE_POOL_SLAB_SIZE
# define DEFAULT_MUSCLE_POOL_SLAB_SIZE (4*1024)  // let's have each slab fit nicely into a 4KB page
#endif

/** An interface that must be implemented by all ObjectPool classes.
  * Used to support polymorphism in pool management.
  */
class AbstractObjectGenerator
{
public:
   /** Default ctor */
   AbstractObjectGenerator() {/* empty */}

   /** Virtual dtor to keep C++ honest */
   virtual ~AbstractObjectGenerator() {/* empty */}

   /** Should be implemented to pass through to ObtainObject(). 
     * Useful for handling different types of ObjectPool interchangably.
     * Note that the caller is responsible for deleting or recycling
     * the returned object!
     */
   virtual void * ObtainObjectGeneric() = 0;
};

/** An interface that must be implemented by all ObjectPool classes.
  * Used to support polymorphism in our reference counting. 
  */
class AbstractObjectRecycler
{
public:
   /** Default constructor.  Registers us with the global recyclers list. */
   AbstractObjectRecycler();

   /** Default destructor.  Unregisters us from the global recyclers list. */
   virtual ~AbstractObjectRecycler();

   /** Should be implemented to downcast (obj) to the correct type,
     * and recycle it (typically by calling ReleaseObject().
     * @param obj Pointer to the object to recycle.  Must point to an object of the correct type.
     *            May be NULL, in which case this method is a no-op.
     */
   virtual void RecycleObject(void * obj) = 0;

   /** Should be implemented to destroy all objects we have cached and
     * return the number of objects that were destroyed.
     */
   virtual uint32 FlushCachedObjects() = 0;

   /** Should print this object's state to stdout.  Used for debugging. */
   virtual void PrintToStream() const = 0;

   /** Walks the linked list of all AbstractObjectRecyclers, calling
     * FlushCachedObjects() on each one, until all cached objects have been destroyed.
     * This method is called by the SetupSystem destructor, to ensure that
     * all objects have been deleted before main() returns, so that shutdown
     * ordering issues do not cause bad memory writes, etc.
     */
   static void GlobalFlushAllCachedObjects();

   /** Prints information about the AbstractObjectRecyclers to stdout. */
   static void GlobalPrintRecyclersToStream();

private:
   AbstractObjectRecycler * _prev;
   AbstractObjectRecycler * _next;
};

/** This class is just here to usefully tie together the object generating and
  * object recycling capabilities of its two superclasses into a single interface.
  */
class AbstractObjectManager : public AbstractObjectGenerator, public AbstractObjectRecycler
{
   // empty
};

/** A thread-safe templated object pooling class that helps reduce the number of 
 *  dynamic allocations and deallocations in your app.  Instead of calling 'new Object', 
 *  you call myObjectPool.ObtainObject(), and instead of calling 'delete Object', you call
 *  myObjectPool.ReleaseObject().  The advantage is that the ObjectPool will
 *  keep (up to a certain number of) "spare" Objects around, and recycle them back
 *  to you as needed. 
 */
template <class Object, int MUSCLE_POOL_SLAB_SIZE=DEFAULT_MUSCLE_POOL_SLAB_SIZE> class ObjectPool : public AbstractObjectManager
{
public:
   /**
    *  Constructor.
    *  @param maxPoolSize the approximate maximum number of recycled objects that may be kept around for future reuse at any one time.  Defaults to 100.
    */      
   ObjectPool(uint32 maxPoolSize=100) : _curPoolSize(0), _maxPoolSize(maxPoolSize), _firstSlab(NULL), _lastSlab(NULL)
   {
      MASSERT(NUM_OBJECTS_PER_SLAB<=65535, "Too many objects per ObjectSlab, uint16 indices will overflow!");
   }

   /** 
    *  Destructor.  Deletes all objects in the pool.
    */
   virtual ~ObjectPool()
   {
      while(_firstSlab)
      {
         if (_firstSlab->IsInUse()) 
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "~ObjectPool %p (%s):  slab %p is still in use when we destroy it!\n", this, _firstSlab->GetObjectClassName(), _firstSlab);
            _firstSlab->PrintToStream();
            MCRASH("ObjectPool destroyed while its objects were still in use (CompleteSetupSystem object not declared at the top of main(), or Ref objects were leaked?)");
         }
         ObjectSlab * nextSlab = _firstSlab->GetNext();
         delete _firstSlab;
         _firstSlab = nextSlab;
      }
   }

   /** Returns a new Object for use (or NULL if no memory available).
    *  You are responsible for calling ReleaseObject() on this object
    *  when you are done with it.
    *  This method is thread-safe.
    *  @return a new Object, or NULL if out of memory.
    */
   Object * ObtainObject()
   {
#ifdef DISABLE_OBJECT_POOLING
      Object * ret = newnothrow Object;
      if (ret) ret->SetManager(this);
          else WARN_OUT_OF_MEMORY;
      return ret;
#else
      Object * ret = NULL;
      if (_mutex.Lock() == B_NO_ERROR)
      {
         ret = ObtainObjectAux();
         _mutex.Unlock();
      }
      if (ret) ret->SetManager(this);
          else WARN_OUT_OF_MEMORY;
      return ret;
#endif
   }

   /** Adds the given object to our "standby" object list to be recycled, or
    *  deletes it if the "standby" list is already at its maximum size.
    *  This method is thread-safe.
    *  @param obj An Object to recycle or delete.  May be NULL.  
    */
   void ReleaseObject(Object * obj)
   {
      if (obj)
      {
         MASSERT(obj->GetManager()==this, "ObjectPool::ReleaseObject was passed an object that it never allocated!");
         *obj = GetDefaultObject();  // necessary so that e.g. if (obj) is holding any Refs, it will release them now
         obj->SetManager(NULL);

#ifdef DISABLE_OBJECT_POOLING
         delete obj;
#else
         if (_mutex.Lock() == B_NO_ERROR)
         {
            ObjectSlab * slabToDelete = ReleaseObjectAux(obj);
            _mutex.Unlock();
            delete slabToDelete;  // do this outside the critical section, for better concurrency
         }
         else WARN_OUT_OF_MEMORY;  // critical error -- not really out of memory but still
#endif
      }
   }

   /** AbstractObjectGenerator API:  Useful for polymorphism */
   virtual void * ObtainObjectGeneric() {return ObtainObject();}

   /** AbstractObjectRecycler API:  Useful for polymorphism */
   virtual void RecycleObject(void * obj) {ReleaseObject((Object *)obj);}
    
   /** Implemented to call Drain() and return the number of objects drained. */
   virtual uint32 FlushCachedObjects() {uint32 ret = 0; (void) Drain(&ret); return ret;}

   /** Returns the name of the class of objects this pool is designed to hold. */
   const char * GetObjectClassName() const {return typeid(Object).name();}

   /** Prints this object's state to stdout.  Used for debugging. */
   virtual void PrintToStream() const
   {
      uint32 numSlabs            = 0;
      uint32 minItemsInUseInSlab = MUSCLE_NO_LIMIT;
      uint32 maxItemsInUseInSlab = 0;
      uint32 totalItemsInUse     = 0;
      if (_mutex.Lock() == B_NO_ERROR)
      {
         const ObjectSlab * slab = _firstSlab;
         while(slab)
         {
            numSlabs++;
            slab->GetUsageStats(minItemsInUseInSlab, maxItemsInUseInSlab, totalItemsInUse);
            slab = slab->GetNext();
         }
         _mutex.Unlock();
      }
      if (minItemsInUseInSlab == MUSCLE_NO_LIMIT) minItemsInUseInSlab = 0;  // just to avoid questions

      uint32 slabSizeItems = NUM_OBJECTS_PER_SLAB;
      printf("ObjectPool<%s> contains " UINT32_FORMAT_SPEC " " UINT32_FORMAT_SPEC "-slot slabs, with " UINT32_FORMAT_SPEC " total items in use (%.1f%% loading, " UINT32_FORMAT_SPEC " total bytes).   LightestSlab=" UINT32_FORMAT_SPEC ", HeaviestSlab=" UINT32_FORMAT_SPEC " (" UINT32_FORMAT_SPEC " bytes per item)\n", GetObjectClassName(), numSlabs, slabSizeItems, totalItemsInUse, (numSlabs>0)?(100.0f*(((float)totalItemsInUse)/(numSlabs*slabSizeItems))):0.0f, (uint32)(numSlabs*sizeof(ObjectSlab)), minItemsInUseInSlab, maxItemsInUseInSlab, (uint32) sizeof(Object));
   }

   /** Removes all "spare" objects from the pool and deletes them. 
     * This method is thread-safe.
     * @param optSetNumDrained If non-NULL, this value will be set to the number of objects destroyed.
     * @returns B_NO_ERROR on success, or B_ERROR if it couldn't lock the lock for some reason.
     */
   status_t Drain(uint32 * optSetNumDrained = NULL)
   {
      if (_mutex.Lock() == B_NO_ERROR)
      {
         // This will be our linked list of slabs to delete, later
         ObjectSlab * toDelete = NULL;

         // Pull out all the slabs that are not currently in use
         ObjectSlab * slab = _firstSlab;
         while(slab)
         {
            ObjectSlab * nextSlab = slab->GetNext();
            if (slab->IsInUse() == false)
            {
               slab->RemoveFromSlabList();
               slab->SetNext(toDelete);
               toDelete = slab;
               _curPoolSize -= NUM_OBJECTS_PER_SLAB;
            }
            slab = nextSlab;
         }
         (void) _mutex.Unlock();

         // Do the actual slab deletions outside of the critical section, for better concurrency
         uint32 numObjectsDeleted = 0;
         while(toDelete)
         {
            ObjectSlab * nextSlab = toDelete->GetNext();
            numObjectsDeleted += NUM_OBJECTS_PER_SLAB;

            delete toDelete;
            toDelete = nextSlab;
         }

         if (optSetNumDrained) *optSetNumDrained = numObjectsDeleted;
         return B_NO_ERROR;
      }
      else return B_ERROR;
   }

   /** Returns the maximum number of "spare" objects that will be kept
     * in the pool, ready to be recycled.  This is the value that was
     * previously set either in the constructor or by SetMaxPoolSize().
     */
   uint32 GetMaxPoolSize() const {return _maxPoolSize;}

   /** Sets a new maximum size for this pool.  Note that changing this
     * value will not cause any object to be added or removed to the
     * pool immediately;  rather the new size will be enforced only
     * on future operations.
     */
   void SetMaxPoolSize(uint32 mps) {_maxPoolSize = mps;}

   /** Returns a read-only reference to a persistent Object that is default-constructed. */
   const Object & GetDefaultObject() const {return GetDefaultObjectForType<Object>();}

   /** Returns the total number of bytes currently taken up by this ObjectPool.
     * The returned value is computed by calling GetTotalDataSize() on each object
     * in turn (including objects in use and objects in reserve).  Note that this
     * method will only compile if the Object type has a GetTotalDataSize() method.
     */
   uint32 GetTotalDataSize() const 
   {
      uint32 ret = sizeof(*this);
      if (_mutex.Lock() == B_NO_ERROR)
      {
         ObjectSlab * slab = _firstSlab;
         while(slab)
         {
            ret += slab->GetTotalDataSize();
            slab = slab->GetNext();
         }
         _mutex.Unlock();
      }
      return ret;
   }

   /** Returns the total number of items currently allocated by this ObjectPool.
     * Note that the returned figure includes both objects in use and objects in reserve.
     */
   uint32 GetNumAllocatedItemSlots() const 
   {
      uint32 ret = 0;
      if (_mutex.Lock() == B_NO_ERROR)
      {
         ObjectSlab * slab = _firstSlab;
         while(slab)
         {
            ret += NUM_OBJECTS_PER_SLAB;
            slab = slab->GetNext();
         }
         _mutex.Unlock();
      }
      return ret;
   }

private:
   Mutex _mutex;

   class ObjectSlab;

   enum {INVALID_NODE_INDEX = ((uint16)-1)};  // the index-version of a NULL pointer

   class ObjectNode : public Object
   {
   public:
      ObjectNode() : _arrayIndex(INVALID_NODE_INDEX), _nextIndex(INVALID_NODE_INDEX) {/* empty */}

      void SetArrayIndex(uint16 arrayIndex) {_arrayIndex = arrayIndex;}
      uint16 GetArrayIndex() const {return _arrayIndex;}

      void SetNextIndex(uint16 nextIndex) {_nextIndex = nextIndex;}
      uint16 GetNextIndex() const {return _nextIndex;}

      const char * GetObjectClassName() const {return typeid(Object).name();}
 
   private:
      uint16 _arrayIndex;
      uint16 _nextIndex;   // only valid when we are in the free list 
   };

   friend class ObjectSlab;  // for VC++ compatibility, this must be here

   /** The other member items of the ObjectSlab class are held here, so that we can call sizeof(ObjectSlabData) to determine the proper array length */
   class ObjectSlabData
   {
   public:
      ObjectSlabData(ObjectPool * pool) : _pool(pool), _prev(NULL), _next(NULL), _firstFreeNodeIndex(INVALID_NODE_INDEX), _numNodesInUse(0) {/* empty */}

      /** Returns true iff there is at least one ObjectNode available in this slab. */
      bool HasAvailableNodes() const {return (_firstFreeNodeIndex != INVALID_NODE_INDEX);}

      /** Returns true iff there is at least one ObjectNode in use in this slab. */
      bool IsInUse() const {return (_numNodesInUse > 0);}

      void RemoveFromSlabList()
      {
         (_prev ? _prev->_data._next : _pool->_firstSlab) = _next;
         (_next ? _next->_data._prev : _pool->_lastSlab)  = _prev;
      }

      void AppendToSlabList(ObjectSlab * slab)
      {
         _prev = _pool->_lastSlab;
         _next = NULL;
         (_prev ? _prev->_data._next : _pool->_firstSlab) = _pool->_lastSlab = slab;
      }

      void PrependToSlabList(ObjectSlab * slab)
      {
         _prev = NULL;
         _next = _pool->_firstSlab;
         (_next ? _next->_data._prev : _pool->_lastSlab) = _pool->_firstSlab = slab;
      }

      uint16 GetFirstFreeNodeIndex() const {return _firstFreeNodeIndex;}
      uint16 GetNumNodesInUse()      const {return _numNodesInUse;}

      void PopObjectNode(ObjectNode * node)
      {
         _firstFreeNodeIndex = node->GetNextIndex();
         ++_numNodesInUse;
         node->SetNextIndex(INVALID_NODE_INDEX);  // so that it will be seen as 'in use' by the debug/assert code
      }

      /** Adds the specified node back to our free-nodes list. */
      void PushObjectNode(ObjectNode * node)
      {
         node->SetNextIndex(_firstFreeNodeIndex);
         _firstFreeNodeIndex = node->GetArrayIndex(); 
         --_numNodesInUse;
      }

      void InitializeObjectNode(ObjectNode * n, uint16 i)
      {
         n->SetArrayIndex(i);
         n->SetNextIndex(_firstFreeNodeIndex);
         _firstFreeNodeIndex = i;
      }

      void SetNext(ObjectSlab * next) {_next = next;}
      ObjectSlab * GetNext() const {return _next;}

      void GetUsageStats(uint32 & min, uint32 & max, uint32 & total) const
      {
         min = muscleMin(min, (uint32)_numNodesInUse);
         max = muscleMax(max, (uint32)_numNodesInUse);
         total += _numNodesInUse;
      }

   private:
      ObjectPool * _pool;
      ObjectSlab * _prev;
      ObjectSlab * _next;
      uint16 _firstFreeNodeIndex;
      uint16 _numNodesInUse;
   };

   // All the (int) casts are here so that it the user specifies a slab size of zero, we will get a negative
   // number and not a very large positive number that crashes the compiler!
   enum {
      NUM_OBJECTS_PER_SLAB_AUX = (((int)MUSCLE_POOL_SLAB_SIZE-(int)sizeof(ObjectSlabData))/(int)sizeof(ObjectNode)),
      NUM_OBJECTS_PER_SLAB     = (NUM_OBJECTS_PER_SLAB_AUX>1)?NUM_OBJECTS_PER_SLAB_AUX:1
   };
   
   class ObjectSlab
   {
   public:
      // Note that _prev and _next are deliberately not set here... we don't use them until we are added to the list
      ObjectSlab(ObjectPool * pool) : _data(pool)
      {
         MASSERT((reinterpret_cast<ObjectSlab *>(_nodes) == this), "ObjectSlab:  _nodes array isn't located at the beginning of the ObjectSlab!  ReleaseObjectAux()'s reinterpret_cast won't work correctly!");
         for (uint16 i=0; i<NUM_OBJECTS_PER_SLAB; i++) _data.InitializeObjectNode(&_nodes[i], i);
      }

      // Note:  this method assumes a node is available!  Don't call it without
      //        calling HasAvailableNodes() first to check, or you will crash!
      ObjectNode * ObtainObjectNode() 
      {
         ObjectNode * node = &_nodes[_data.GetFirstFreeNodeIndex()];
         _data.PopObjectNode(node); 
         return node;
      }

      void ReleaseObjectNode(ObjectNode * node) {_data.PushObjectNode(node);}

      void SetNext(ObjectSlab * next) {_data.SetNext(next);}
      ObjectSlab * GetNext() const {return _data.GetNext();}

      void PrintToStream() const
      {
         printf("   ObjectSlab %p:  %u nodes in use\n", this, _data.GetNumNodesInUse());
         for (uint32 i=0; i<NUM_OBJECTS_PER_SLAB; i++)
         {
            const ObjectNode * n = &_nodes[i];
            if (n->GetNextIndex() == INVALID_NODE_INDEX) printf("      " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":   %s %p is possibly still in use?\n", i, (uint32)NUM_OBJECTS_PER_SLAB, GetObjectClassName(), n);
         }
      }

      const char * GetObjectClassName() const {return _nodes[0].GetObjectClassName();}

      bool HasAvailableNodes() const {return _data.HasAvailableNodes();}
      bool IsInUse() const           {return _data.IsInUse();}
      void RemoveFromSlabList()      {_data.RemoveFromSlabList();}
      void AppendToSlabList()        {_data.AppendToSlabList(this);}
      void PrependToSlabList()       {_data.PrependToSlabList(this);}

      uint32 GetTotalDataSize() const
      {
         uint32 ret = sizeof(_data);
         for (uint32 i=0; i<ARRAYITEMS(_nodes); i++) ret += _nodes[i].GetTotalDataSize();
         return ret;
      }

      void GetUsageStats(uint32 & min, uint32 & max, uint32 & total) const
      {
         _data.GetUsageStats(min, max, total);
      }

   private:
      friend class ObjectSlabData;

      ObjectNode _nodes[NUM_OBJECTS_PER_SLAB];  // NOTE:  THIS MUST BE THE FIRST MEMBER ITEM so that we can cast (&_nodes[0]) to (ObjectSlab *)
      ObjectSlabData _data;                     // must be declared after _nodes!  That's why ObjectSlab can't just inherit from this class
      // Any other member variables should be added to the ObjectSlabData class rather than here, so that we can calculate NUM_OBJECTS_PER_SLAB correctly
   };

   // Must be called with _mutex locked!   Returns either NULL, or a pointer to a
   // newly allocated Object.
   Object * ObtainObjectAux()
   {
      Object * ret = NULL;
      if ((_firstSlab)&&(_firstSlab->HasAvailableNodes()))
      {
         ret = _firstSlab->ObtainObjectNode();
         if ((_firstSlab->HasAvailableNodes() == false)&&(_firstSlab != _lastSlab))
         {
            // Move _firstSlab out of the way (to the end of the slab list) for next time
            ObjectSlab * tmp = _firstSlab;  // use temp var since _firstSlab will change
            tmp->RemoveFromSlabList();
            tmp->AppendToSlabList();
         }
      }
      else
      {
         // Hmm, we must have run out out of non-full slabs.  Create a new slab and use it.
         ObjectSlab * slab = newnothrow ObjectSlab(this);
         if (slab)
         {
            ret = slab->ObtainObjectNode();  // guaranteed not to fail, since slab is new
            if (slab->HasAvailableNodes()) slab->PrependToSlabList();
                                      else slab->AppendToSlabList();  // could happen, if NUM_OBJECTS_PER_SLAB==1
            _curPoolSize += NUM_OBJECTS_PER_SLAB;
         }
         // we'll do the WARN_OUT_OF_MEMORY below, outside the mutex lock
      }
      if (ret) --_curPoolSize;
      return ret;
   }

   // Must be called with _mutex locked!   Returns either NULL, or a pointer 
   // an ObjectSlab that should be deleted outside of the critical section.
   ObjectSlab * ReleaseObjectAux(Object * obj)
   {
      ObjectNode * objNode = static_cast<ObjectNode *>(obj);
      ObjectSlab * objSlab = reinterpret_cast<ObjectSlab *>(objNode-objNode->GetArrayIndex());

      objSlab->ReleaseObjectNode(objNode);  // guaranteed to work, since we know (obj) is in use in (objSlab)

      if ((++_curPoolSize > (_maxPoolSize+NUM_OBJECTS_PER_SLAB))&&(objSlab->IsInUse() == false))
      {
         _curPoolSize -= NUM_OBJECTS_PER_SLAB;
         objSlab->RemoveFromSlabList();
         return objSlab;
      }
      else if (objSlab != _firstSlab) 
      {
         objSlab->RemoveFromSlabList();
         objSlab->PrependToSlabList();
      }
      return NULL;
   }

   uint32 _curPoolSize;  // tracks the current number of "available" objects
   uint32 _maxPoolSize;  // the maximum desired number of "available" objects
   ObjectSlab * _firstSlab;
   ObjectSlab * _lastSlab;
};

}; // end namespace muscle

#endif
