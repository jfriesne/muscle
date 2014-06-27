/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePulseNode_h
#define MusclePulseNode_h

#include "util/TimeUtilityFunctions.h"
#include "util/CountedObject.h"

namespace muscle {

class PulseNode;
class PulseNodeManager;

/** Interface class for any object that can schedule Pulse() calls for itself via 
 *  its PulseNodeManager.   (Typically the PulseNodeManager role is played by
 *  the ReflectServer class)
 */
class PulseNode : private CountedObject<PulseNode>
{
public:
   /** Default constructor */
   PulseNode();

   /** Destructor.  Does not delete any attached child PulseNodes. */
   virtual ~PulseNode();

protected:
   /** This class is used to encapsulate the arguments to GetPulseTime() and Pulse(), so that
     * there is less data to place on the stack in each callback call.
     */
   class PulseArgs
   {
   public:
      /** Returns the approximate time (in microseconds) at which our Pulse() method was called.  
        * Calling this method is cheaper than calling GetRunTime64() directly.
        */
      uint64 GetCallbackTime() const {return _callTime;}

      /** Returns the time (in microseconds) at which our Pulse() method was supposed to be called at.  
        * Note that the actual call time (as returned by GetCallbackTime() will generally be a bit larger
        * than the value returned by GetScheduledTime(), as computers are not infinitely fast and therefore
        * they will have some latency before scheduled calls are executed.
        */
      uint64 GetScheduledTime() const {return _prevTime;}

   private:
      friend class PulseNode;

      PulseArgs(uint64 callTime, uint64 prevTime) : _callTime(callTime), _prevTime(prevTime) {/* empty */}

      uint64 _callTime; 
      uint64 _prevTime; 
   };

public:
   /**
    * This method can be overridden to tell the PulseNodeManager when we would like
    * to have our Pulse() method called.  This method is guaranteed to be called only
    * during the following times:
    * <ol>
    *   <li>When the PulseNode is first probed by the PulseNodeManager</li>
    *   <li>Immediately after our Pulse() method has returned</li>
    *   <li>Soon after InvalidatePulseTime() has been called one or more times 
    *       (InvalidatePulseTime() calls are merged together for efficiency)</li>
    * </ol>
    * The default implementation always returns MUSCLE_TIME_NEVER.
    *
    * @param args Args is a reference to an object containing the following context
    *             information regarding this call:
    *
    *           args.GetCallbackTime() The current wall-clock time-value, in microseconds.
    *                                   This will be roughly the same value as returned by
    *                                   GetRunTime64(), but it's cheaper to call this method.
    *           args.GetScheduledTime() The value that this method returned the last time it was
    *                                   called.  The very first time this method is called, this value
    *                                   will be passed in as MUSCLE_TIME_NEVER.
    * @return Return MUSCLE_TIME_NEVER if you don't wish to schedule a future call to Pulse();
    *         or return the time at which you want Pulse() to be called.  Returning values less
    *         than or equal to (now) will cause Pulse() to be called as soon as possible.
    */
   virtual uint64 GetPulseTime(const PulseArgs & args);

   /**
    * Will be called at the time specified previously by GetPulseTime().  GetPulseTime()
    * will be called again immediately after this call, to check if you want to schedule
    * another Pulse() call for later.
    * Default implementation is a no-op.
    *
    * @param args Args is a reference to an object containing the following context
    *             information regarding this call:
    *
    *           args.GetCallbackTime() The current wall-clock time-value, in microseconds.
    *                                   This will be roughly the same value as returned by
    *                                   GetRunTime64(), but it's cheaper to call this method.
    *           args.GetScheduledTime() The time this Pulse() call was scheduled to occur at, in
    *                      microseconds, as previously returned by GetPulseTime(). Note
    *                      that unless your computer is infinitely fast, this time will
    *                      always be at least a bit less than (now), since there is a delay 
    *                      between when the program gets woken up to service the next Pulse() 
    *                      call, and when the call actually happens.  (you may be able to 
    *                      use this value to compensate for the slippage, if it bothers you)
    */
   virtual void Pulse(const PulseArgs & args);

   /** 
    *  Adds the given child into our set of child PulseNodes.  Any PulseNode in our
    *  set of children will have its pulsing needs taken care of by us, but it is
    *  not considered "owned" by this PulseNode--it will not be deleted when we are.
    *  @param child The child to place into our set of child PulseNodes.
    *  @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory).
    */
   status_t PutPulseChild(PulseNode * child);

   /** Attempts to remove the given child from our set of child PulseNodes.
    *  @param child The child to remove
    *  @returns B_NO_ERROR on success, or B_ERROR on failure (child wasn't in our set)
    */
   status_t RemovePulseChild(PulseNode * child);

   /** Removes all children from our set of child PulseNodes */
   void ClearPulseChildren();

   /** Returns true iff the given child is in our set of child PulseNodes. 
     * @param child the child to look for.
     */
   bool ContainsPulseChild(PulseNode * child) const {return ((child)&&(child->_parent == this));}

   /** Returns when this object wants its call to Pulse() scheduled next, or MUSCLE_TIME_NEVER 
    *  if it has no call to Pulse() currently scheduled. 
    */
   uint64 GetScheduledPulseTime() const {return _myScheduledTime;}

   /** Returns the run-time at which the PulseNodeManager started calling our callbacks.
    *  Useful for any object that wants to limit the maximum duration of its timeslice
    *  in the PulseNodeManager's event loop.
    */
   uint64 GetCycleStartTime() const {return _parent ? _parent->GetCycleStartTime() : _cycleStartedAt;}

   /** Sets the maximum number of microseconds that this class should allow its callback
    *  methods to execute for (relative to the cycle start time, as shown above).  Note 
    *  that this value is merely a suggestion;  it remains up to the subclass's callback 
    *  methods to ensure that the suggestion is actually followed.
    *  @param maxUsecs Maximum number of microseconds that the time slice should last for.
    *                  If set to MUSCLE_TIME_NEVER, that indicates that there is no suggested limit.
    */
   void SetSuggestedMaximumTimeSlice(uint64 maxUsecs) {_maxTimeSlice = maxUsecs; _timeSlicingSuggested = (_maxTimeSlice != MUSCLE_TIME_NEVER);}

   /** Returns the current suggested maximum duration of our time slice, or MUSCLE_TIME_NEVER
    *  if there is no suggested limit.  Default value is MUSCLE_TIME_NEVER.
    */
   uint64 GetSuggestedMaximumTimeSlice() const {return _maxTimeSlice;}

   /** Convenience method -- returns true iff the current value of the run-time 
    *  clock (GetRunTime64()) indicates that our suggested time slice has expired.  
    *  This method is cheap to call often.
    */
   bool IsSuggestedTimeSliceExpired() const {return ((_timeSlicingSuggested)&&(GetRunTime64() >= (_cycleStartedAt+_maxTimeSlice)));}

   /**
    * Sets a flag to indicate that GetPulseTime() should be called on this object.
    * Call this whenever you've decided to reschedule your pulse time outside
    * of a Pulse() event.
    * @param clearPrevResult if true, this call will also clear the stored prevResult
    *                        value, so that the next time GetPulseTime() is called,
    *                        prevResult is passed in as MUSCLE_TIME_NEVER.  If false,
    *                        the prevResult value will be left alone.
    */
   void InvalidatePulseTime(bool clearPrevResult = true); 

   /** Returns a pointer to this PulseNode's parent PulseNode, if any. */
   PulseNode * GetPulseParent() const {return _parent;}

private:
   void ReschedulePulseChild(PulseNode * child, int toList);
   uint64 GetFirstScheduledChildTime() const {return _firstChild[LINKED_LIST_SCHEDULED] ? _firstChild[LINKED_LIST_SCHEDULED]->_aggregatePulseTime : MUSCLE_TIME_NEVER;}
   void GetPulseTimeAux(uint64 now, uint64 & min);
   void PulseAux(uint64 now);

   // Sets the cycle-started-at time for this object, as returned by GetCycleStartTime().
   void SetCycleStartTime(uint64 st) {_cycleStartedAt = st;}

   PulseNode * _parent;

   uint64 _aggregatePulseTime;  // time when we need PulseAux() to be called next
   uint64 _myScheduledTime;     // time when our own personal Pulse() should be called next
   uint64 _cycleStartedAt;      // time when the PulseNodeManager started serving us.
   bool _myScheduledTimeValid;  // true iff _myScheduledTime doesn't need to be recalculated

   // Linked list that this node is in (or NULL if we're not in any linked list)
   int _curList;                // index of the list we are part of, or -1 if we're not in any list
   PulseNode * _prevSibling;
   PulseNode * _nextSibling;

   enum {
      LINKED_LIST_SCHEDULED = 0,  // list of children with known upcoming pulse-times (sorted)
      LINKED_LIST_UNSCHEDULED,    // list of children with known MUSCLE_TIME_NEVER pulse-times (unsorted)
      LINKED_LIST_NEEDSRECALC,    // list of children whose pulse-times need to be recalculated (unsorted)
      NUM_LINKED_LISTS
   };

   // Linked list of child nodes with finite pulse times, sorted by their pulse times
   PulseNode * _firstChild[NUM_LINKED_LISTS];
   PulseNode * _lastChild[NUM_LINKED_LISTS];

   uint64 _maxTimeSlice;
   bool _timeSlicingSuggested;

   friend class PulseNodeManager;
};

/** Subclasses of this class are allowed to manage PulseNode objects by 
  * calling their GetPulseTimeAux() and PulseAux() methods (indirectly).  
  * Most code won't need to use this class.
  */
class PulseNodeManager
{
public:
   /** Default constructor */
   PulseNodeManager() {/* empty */}

   /** Destructor */
   ~PulseNodeManager() {/* empty */}

protected:
   /** Passes the call on through to the given PulseNode */
   inline void CallGetPulseTimeAux(PulseNode & p, uint64 now, uint64 & min) const {p.GetPulseTimeAux(now, min);}

   /** Passes the call on through to the given PulseNode */
   inline void CallPulseAux(PulseNode & p, uint64 now) const {if (now >= p._aggregatePulseTime) p.PulseAux(now);}

   /** Passes the call on through to the given PulseNode */
   inline void CallSetCycleStartTime(PulseNode & p, uint64 now) const {p.SetCycleStartTime(now);}
};

}; // end namespace muscle

#endif
