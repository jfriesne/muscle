/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "util/PulseNode.h"

namespace muscle {

PulseNode :: PulseNode() : _parent(NULL), _aggregatePulseTime(MUSCLE_TIME_NEVER), _myScheduledTime(MUSCLE_TIME_NEVER), _cycleStartedAt(0), _myScheduledTimeValid(false), _curList(-1), _prevSibling(NULL), _nextSibling(NULL), _maxTimeSlice(MUSCLE_TIME_NEVER), _timeSlicingSuggested(false)
{
   for (uint32 i=0; i<NUM_LINKED_LISTS; i++) _firstChild[i] = _lastChild[i] = NULL;
}

PulseNode :: ~PulseNode() 
{
   // unlink everybody, but don't delete anyone; no ownership is implied here!
   if (_parent) _parent->RemovePulseChild(this);
   ClearPulseChildren();
}

uint64 PulseNode :: GetPulseTime(const PulseArgs &) 
{
   return MUSCLE_TIME_NEVER;
}

void PulseNode :: Pulse(const PulseArgs &) 
{
   // empty
}

void PulseNode :: InvalidatePulseTime(bool clearPrevResult)
{
   if (_myScheduledTimeValid)
   {
      _myScheduledTimeValid = false;
      if (clearPrevResult) _myScheduledTime = MUSCLE_TIME_NEVER;
      if (_parent) _parent->ReschedulePulseChild(this, LINKED_LIST_NEEDSRECALC);
   }
}

void PulseNode :: GetPulseTimeAux(uint64 now, uint64 & min)
{
   // First, update myself, if necessary...
   if (_myScheduledTimeValid == false) 
   {
      _myScheduledTimeValid = true;
      _myScheduledTime = GetPulseTime(PulseArgs(now, _myScheduledTime));
   }

   // Then handle any of my kids who need to be recalculated also
   PulseNode * & firstNeedy = _firstChild[LINKED_LIST_NEEDSRECALC];
   if (firstNeedy) while(firstNeedy) firstNeedy->GetPulseTimeAux(now, min);  // guaranteed to move (firstNeedy) out of the recalc list!

   // Recalculate our effective pulse time
   uint64 oldAggregatePulseTime = _aggregatePulseTime;
   _aggregatePulseTime = muscleMin(_myScheduledTime, GetFirstScheduledChildTime());
   if ((_parent)&&((_curList == LINKED_LIST_NEEDSRECALC)||(_aggregatePulseTime != oldAggregatePulseTime))) _parent->ReschedulePulseChild(this, (_aggregatePulseTime==MUSCLE_TIME_NEVER)?LINKED_LIST_UNSCHEDULED:LINKED_LIST_SCHEDULED);

   // Update the caller's minimum time value
   if (_aggregatePulseTime < min) min = _aggregatePulseTime;
}

void PulseNode :: PulseAux(uint64 now)
{
   if ((_myScheduledTimeValid)&&(now >= _myScheduledTime)) 
   {
      Pulse(PulseArgs(now, _myScheduledTime));
      _myScheduledTimeValid = false;
   }

   PulseNode * p = _firstChild[LINKED_LIST_SCHEDULED];
   while((p)&&(now >= p->_aggregatePulseTime))
   {
      p->PulseAux(now);  // guaranteed to move (p) to our NEEDSRECALC list
      p = _firstChild[LINKED_LIST_SCHEDULED];  // and move on to the next scheduled child
   }

   // Make sure we get recalculated no matter what (because we know something happened)
   if (_parent) _parent->ReschedulePulseChild(this, LINKED_LIST_NEEDSRECALC);
}

status_t PulseNode :: PutPulseChild(PulseNode * child)
{
   if (child->_parent) child->_parent->RemovePulseChild(child);
   child->_parent = this;
   ReschedulePulseChild(child, LINKED_LIST_NEEDSRECALC);
   return B_NO_ERROR;
}

status_t PulseNode :: RemovePulseChild(PulseNode * child)
{
   if (child->_parent == this)
   {
      bool doResched = (child == _firstChild[LINKED_LIST_SCHEDULED]);
      ReschedulePulseChild(child, -1);
      child->_parent = NULL;
      child->_myScheduledTimeValid = false;
      if ((doResched)&&(_parent)) _parent->ReschedulePulseChild(this, LINKED_LIST_NEEDSRECALC);
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

void PulseNode :: ClearPulseChildren()
{
   for (uint32 i=0; i<NUM_LINKED_LISTS; i++) while(_firstChild[i]) (void) RemovePulseChild(_firstChild[i]);
}

void PulseNode :: ReschedulePulseChild(PulseNode * child, int whichList)
{
   int cl = child->_curList;
   if ((whichList != cl)||(cl == LINKED_LIST_SCHEDULED))  // since we may need to move within the scheduled list
   {
      // First, remove the child from any list he may currently be in
      if (cl >= 0)
      {
         if (child->_prevSibling) child->_prevSibling->_nextSibling = child->_nextSibling;
         if (child->_nextSibling) child->_nextSibling->_prevSibling = child->_prevSibling;
         if (child == _firstChild[cl]) _firstChild[cl] = child->_nextSibling;
         if (child == _lastChild[cl])  _lastChild[cl]  = child->_prevSibling;
         child->_prevSibling = child->_nextSibling = NULL;
      }

      child->_curList = whichList;
      switch(whichList)
      {
         case LINKED_LIST_SCHEDULED:
         {
            PulseNode * p = _firstChild[whichList];
            if (p)
            {
               if (child->_aggregatePulseTime >= _lastChild[whichList]->_aggregatePulseTime)
               {
                  // Shortcut:  append to the tail of the list!
                  child->_prevSibling = _lastChild[whichList];
                  _lastChild[whichList]->_nextSibling = child;
                  _lastChild[whichList] = child;
               }
               else
               {
                  // Worst case:  O(N) walk through the list to find the place to insert (child)
                  while(p->_aggregatePulseTime < child->_aggregatePulseTime) p = p->_nextSibling;

                  // insert (child) just before (p)
                  child->_nextSibling = p;
                  child->_prevSibling = p->_prevSibling;
                  if (p->_prevSibling) p->_prevSibling->_nextSibling = child;
                                  else _firstChild[whichList] = child;  // FogBugz #4092(b)
                  p->_prevSibling = child;
               }
            }
            else _firstChild[whichList] = _lastChild[whichList] = child;
         }
         break;

         case LINKED_LIST_NEEDSRECALC:
            if (_parent) _parent->ReschedulePulseChild(this, LINKED_LIST_NEEDSRECALC);  // if our child is rescheduled that reschedules us too!
         case LINKED_LIST_UNSCHEDULED: 
         {
            // These lists are unsorted, so we can just quickly append the child to the head of the list
            if (_firstChild[whichList])
            {
               child->_nextSibling = _firstChild[whichList];
               _firstChild[whichList]->_prevSibling = child;
               _firstChild[whichList] = child;
            }
            else _firstChild[whichList] = _lastChild[whichList] = child;
         }
         break;

         default:
            // do nothing
         break;
      }
   }
}

}; // end namespace muscle
