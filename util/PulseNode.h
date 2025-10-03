/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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
class PulseNode
{
public:
   /** Default constructor */
   PulseNode();

   /** Destructor.  Does not delete any attached child PulseNodes.
     * This destructor will automatically remove this PulseNode from any
     * parent PulseNode it is registered with, and also disassociate itself from
     * any of its child PulseNodes, so you don't need to handle that cleanup yourself.
     */
   virtual ~PulseNode();

protected:
   /** This class is used to encapsulate the arguments to GetPulseTime() and Pulse(), so that
     * there is less data to place on the stack in each callback call.
     */
   class PulseArgs MUSCLE_FINAL_CLASS
   {
   public:
      /** Returns the approximate time (in microseconds) at which our Pulse() method was called.
        * Calling this method is cheaper than calling GetRunTime64() directly.
        */
      MUSCLE_NODISCARD uint64 GetCallbackTime() const {return _callTime;}

      /** Returns the time (in microseconds) at which our Pulse() method was supposed to be called at.
        * Note that the actual call time (as returned by GetCallbackTime() will generally be a bit larger
        * than the value returned by GetScheduledTime(), as computers are not infinitely fast and therefore
        * they will have some latency before scheduled calls are executed.
        */
      MUSCLE_NODISCARD uint64 GetScheduledTime() const {return _prevTime;}

   private:
      friend class PulseNode;

      PulseArgs(uint64 callTime, uint64 prevTime) : _callTime(callTime), _prevTime(prevTime) {/* empty */}

      uint64 _callTime;
      uint64 _prevTime;
   };

public:
   /**
    * This method can be overridden to return a value (in GetRunTime64()-style microseconds)
    * that indicates to the PulseNodeManager/ReflectServer (or our parent PulseNode)
    * when we would next like to have our Pulse() method called.  This method will
    * to be called in the following situations:
    * <ol>
    *   <li>When this PulseNode is first added to the PulseNodeManager/ReflectServer (or parent PulseNode)</li>
    *   <li>Immediately after our Pulse() method has been called</li>
    *   <li>Shortly after InvalidatePulseTime() has been called one or more times on this object</li>
    * </ol>
    *
    * The default implementation always returns MUSCLE_TIME_NEVER, meaning that no calls to Pulse() are desired.
    *
    * @param args Reference to a PulseArgs object containing the following context-information describing this call:
    * <pre>
    *           args.GetCallbackTime() The current wall-clock time-value, in microseconds.
    *                                   This will be roughly the same value as returned by
    *                                   GetRunTime64(), but it's cheaper to call this method.
    *           args.GetScheduledTime() The value that this method returned the last time it was
    *                                   called.  The very first time this method is called, this value
    *                                   will be passed in as MUSCLE_TIME_NEVER.
    * </pre>
    * @return MUSCLE_TIME_NEVER if you don't wish to schedule a future call to Pulse();
    *         or return the time at which you want Pulse() to be called.  Returning values less
    *         than or equal to (args.GetCallbackTime()) will cause Pulse() to be called as soon as possible.
    *
    * @note if you are overriding this method in a subclass, it is always a good idea to
    *       call up to the superclass implementation and return the smaller of your own return
    *       value and the parent class's return-value (e.g.<pre>return muscleMin(myReturnValue, MyParentClass::GetPulseTime(args));</pre>
    */
   MUSCLE_NODISCARD virtual uint64 GetPulseTime(const PulseArgs & args);

   /**
    * Will be called at (or shortly after) the time specified previously by GetPulseTime(), so
    * you can override this method to do whatever task you wanted to perform at that scheduled time.
    *
    * Default implementation is a no-op.
    *
    * @param args Reference to a PulseArgs object containing the following context-information for this call:
    * <pre>
    *     args.GetCallbackTime() The approximate current wall-clock time-value, in microseconds.
    *                             This will be roughly the same value as returned by
    *                             GetRunTime64(), but it's cheaper to call this method.
    *     args.GetScheduledTime() The time this Pulse() call was scheduled to occur at, in
    *                microseconds, as previously returned by GetPulseTime().  Note
    *                that unless your computer is infinitely fast, this time will
    *                always be at least a bit less than (args.GetCallbackTime()), since there is a delay
    *                between when the program gets woken up to service the next Pulse()
    *                call, and when the call actually happens.  (you may be able to
    *                use this value to compensate for the slippage, if that bothers you)
    * </pre>
    *
    * @note GetPulseTime() will be always called again, immediately after this call returns, to find out
    * if you want to schedule another Pulse() call for later.
    *
    */
   virtual void Pulse(const PulseArgs & args);

   /**
    *  Adds the given child into our set of child PulseNodes.  Any PulseNode in our
    *  set of children will have its Pulse()-ing needs taken care of by us, but it is
    *  not considered "owned" by this PulseNode--that is, it will not be deleted when we are.
    *  @param child The child to place into our set of child PulseNodes.
    *  @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    *  @note if (child) is already a child of a different PulseNode, it will be removed from
    *        that PulseNode's children-list (via RemovePulseChild()) before it is added to this one's.
    */
   status_t PutPulseChild(PulseNode * child);

   /** Attempts to remove the given child from our set of child PulseNodes, and disassociate it from us.
    *  @param child The child to remove
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND on failure (ie child wasn't in our current-set of pulse-children)
    */
   status_t RemovePulseChild(PulseNode * child);

   /** Removes all children from our set of child PulseNodes and disassociates us from them. */
   void ClearPulseChildren();

   /** Returns true iff the given child is in our set of child PulseNodes.
     * @param child the child to look for.
     */
   MUSCLE_NODISCARD bool ContainsPulseChild(PulseNode * child) const {return ((child)&&(child->_parent == this));}

   /** Returns the current record of when this PulseNode wants its call to Pulse() scheduled next
    *  (as previously returned by GetPulseTime()), or MUSCLE_TIME_NEVER there is no call to Pulse() currently scheduled for this PulseNode.
    */
   MUSCLE_NODISCARD uint64 GetScheduledPulseTime() const {return _myScheduledTime;}

   /** Returns the run-time at which the PulseNodeManager/ReflectServer started calling our callbacks.
    *  Useful for any object that wants to limit the maximum duration of its timeslice
    *  in the PulseNodeManager/ReflectServer's event loop.
    */
   MUSCLE_NODISCARD uint64 GetCycleStartTime() const {return _parent ? _parent->GetCycleStartTime() : _cycleStartedAt;}

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
   MUSCLE_NODISCARD uint64 GetSuggestedMaximumTimeSlice() const {return _maxTimeSlice;}

   /** Convenience method -- returns true iff the current value of the run-time
    *  clock (GetRunTime64()) indicates that our suggested time slice has expired.
    *  This method is cheap to call often.
    */
   MUSCLE_NODISCARD bool IsSuggestedTimeSliceExpired() const {return ((_timeSlicingSuggested)&&(GetRunTime64() >= (_cycleStartedAt+_maxTimeSlice)));}

   /**
    * Sets a flag to indicate that GetPulseTime() should be called again on this object,
    * at the beginning of the next event-loop cycle.
    *
    * Call this whenever you've decided to reschedule your upcoming pulse-time from
    * anywhere other than your own Pulse() callback.
    * @param clearPrevResult if true, this call will also clear the stored scheduled-pulse-time
    *                        value, so that the next time GetPulseTime() is called,
    *                        args.GetScheduledTime() will return MUSCLE_TIME_NEVER.  If false,
    *                        the scheduled-pulse-time value will be left as-is.
    */
   void InvalidatePulseTime(bool clearPrevResult = true);

   /** Returns a pointer to this PulseNode's parent PulseNode, or NULL if we don't have a parent PulseNode. */
   MUSCLE_NODISCARD PulseNode * GetPulseParent() const {return _parent;}

private:
   void ReschedulePulseChild(PulseNode * child, int toList);
   MUSCLE_NODISCARD uint64 GetFirstScheduledChildTime() const {return _firstChild[LINKED_LIST_SCHEDULED] ? _firstChild[LINKED_LIST_SCHEDULED]->_aggregatePulseTime : MUSCLE_TIME_NEVER;}
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

   // Endpoints of our three linked lists of child nodes
   PulseNode * _firstChild[NUM_LINKED_LISTS];
   PulseNode * _lastChild[NUM_LINKED_LISTS];

   uint64 _maxTimeSlice;
   bool _timeSlicingSuggested;

   friend class PulseNodeManager;

   DECLARE_COUNTED_OBJECT(PulseNode);
};

/** A PulseNodeManager is a class that has access to call CallGetPulseTimeAux() and CallPulseAux()
  * at the appropriate times, for the various PulseNode derived objects that have registered with it for callbacks.
  * Subclasses of this class are allowed to manage PulseNode objects by
  * calling their GetPulseTimeAux() and PulseAux() methods (indirectly).
  * Most code won't need to use this class directly, as the ReflectServer class derives
  * from this class an implements the necessary PulseNodeManager functionality for you.
  */
class PulseNodeManager
{
public:
   /** Default constructor */
   PulseNodeManager() {/* empty */}

   /** Destructor */
   ~PulseNodeManager() {/* empty */}

protected:
   /** Passes the call on through to the given PulseNode
     * @param p the PulseNode to call GetPulseTimeAux() on
     * @param now the approximate current time in microseconds, as returned by GetRunTime64()
     * @param min the current minimum time across all our nodes; GetPulseTimeAux() may make this time smaller if necessary.
     */
   inline void CallGetPulseTimeAux(PulseNode & p, uint64 now, uint64 & min) const {p.GetPulseTimeAux(now, min);}

   /** Passes the call on through to the given PulseNode
     * @param p the PulseNode to call PulseAux() on
     * @param now the approximate current time in microseconds, as returned by GetRunTime64()
     */
   inline void CallPulseAux(PulseNode & p, uint64 now) const {if (now >= p._aggregatePulseTime) p.PulseAux(now);}

   /** Passes the call on through to the given PulseNode
     * @param p the PulseNode to call SetCycleStartTime() on
     * @param now the approximate current time in microseconds, as returned by GetRunTime64()
     */
   inline void CallSetCycleStartTime(PulseNode & p, uint64 now) const {p.SetCycleStartTime(now);}
};

} // end namespace muscle

#endif
