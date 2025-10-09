/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSwapContentsGuard_h
#define MuscleSwapContentsGuard_h

#include "support/NotCopyable.h"
#include "util/DemandConstructedObject.h"

namespace muscle {

/** RAII convenience class for when you want to swap the contents of two objects, and then always swap them back again later.
  * Both the constructor and the destructor of this object will swap out the value of your specified object with a temporary object.
  * @tparam ItemType the type of the object(s) to call SwapContents() (or muscleSwap()) on
  */
template<typename ItemType> class MUSCLE_NODISCARD SwapContentsGuard : private NotCopyable
{
public:
   /** Single-argument constructor.
     * @param swapA a reference to your object whose contents you want to be swapped with a default-constructed object of the same type.
     * @note our destructor will undo the effects of the swap performed by our constructor
     */
   SwapContentsGuard(ItemType & swapA) : _swapA(swapA), _swapB(_tempStorage.GetObject()) {muscleSwap(_swapA, _swapB);}

   /** Two-argument constructor.
     * @param swapA a reference to your object whose contents you want to be swapped with (swapB).
     * @param swapB a reference to your object whose contents you want to be swapped with (swapA).
     * @note our destructor will undo the effects of the swap performed by our constructor
     */
   SwapContentsGuard(ItemType & swapA, ItemType & swapB) : _swapA(swapA), _swapB(swapB) {muscleSwap(_swapA, _swapB);}

   /** Destructor:  Swaps the two objects a second time, so that they reassume their original (pre-swap) state */
   ~SwapContentsGuard() {muscleSwap(_swapA, _swapB);}

private:
   DemandConstructedObject<ItemType> _tempStorage;
   ItemType & _swapA;
   ItemType & _swapB;
};

} // end namespace muscle

#endif
