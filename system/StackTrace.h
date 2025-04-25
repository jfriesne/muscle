/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleStackTrace_h
#define MuscleStackTrace_h

#include "util/OutputPrinter.h"
#include "util/Queue.h"
#include "util/RefCount.h"
#include "util/String.h"

namespace muscle {

#if defined(__APPLE__)
# include "AvailabilityMacros.h"  // so we can find out if this version of MacOS/X is new enough to include backtrace() and friends
#endif

#if (_MSC_VER >= 1300) && !defined(MUSCLE_AVOID_WINDOWS_STACKTRACE)
# define MUSCLE_USE_MSVC_STACKWALKER 1
#elif (defined(__linux__) && !defined(ANDROID)) || (defined(MAC_OS_X_VERSION_10_5) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5))
# define MUSCLE_USE_BACKTRACE 1
#endif

/** This class stores a stack trace for debugging purposes. */
class StackTrace MUSCLE_FINAL_CLASS : public RefCountable
{
public:
   /** Default constructor */
   StackTrace() MUSCLE_NOEXCEPT {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   StackTrace(const StackTrace & rhs);

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   StackTrace(StackTrace && rhs) MUSCLE_NOEXCEPT;

   /** @copydoc DoxyTemplate::operator=(DoxyTemplate &&) */
   StackTrace & operator =(StackTrace && rhs) MUSCLE_NOEXCEPT;
#endif

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   StackTrace & operator =(const StackTrace & rhs) MUSCLE_NOEXCEPT;

   /** Destructor */
   ~StackTrace() {/* empty */}

   /** Captures the calling thread's current stack into our private storage.
     * Any previously-captured stack frames will be implicitly cleared beforehand.
     * @param maxNumFrames the maximum number of frames to capture.  Defaults to 64.
     * @returns B_NO_ERROR on success, or some other error code on failure.
     */
   status_t CaptureStackFrames(uint32 maxNumFrames = 64);

   /** Drops any captured stack frames and sets this object back to its default-initialized state */
   void ClearStackFrames();

   /** Prints our held stack-frames using the specified OutputPrinter object
     * @param p the OutputPrinter to print with
     */
   void Print(const OutputPrinter & p) const;

   /** Returns the number of captured stack frames this object is holding. */
   uint32 GetNumCapturedStackFrames() const;

   /** Static convenience method for capturing the current stack trace and immediately printing it out.
     * @param p the OutputPrinter to print with
     * @param maxNumFrames the maximum number of frames to capture.  Defaults to 64.
     */
   static status_t StaticPrintStackTrace(const OutputPrinter & p, uint32 maxNumFrames = 64);

   /** Swaps the internal state of this object with the state of (swapWithMe)
     * @param swapWithMe the StackTrace to swap with
     */ 
   void SwapContents(StackTrace & swapWithMe) MUSCLE_NOEXCEPT;

private:
#if defined(MUSCLE_USE_BACKTRACE)
   Queue<void *> _stackFrames;
#elif defined(MUSCLE_USE_MSVC_STACKWALKER)
   RefCountableRef _stackWalkerState;
#endif
};
DECLARE_REFTYPES(StackTrace);

}  // end namespace muscle

#endif
