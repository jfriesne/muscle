/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef IncrementalHashCalculator_h
#define IncrementalHashCalculator_h

#ifdef MUSCLE_ENABLE_SSL
# include <openssl/evp.h>
#endif

#include "support/NotCopyable.h"
#include "support/PseudoFlattenable.h"
#include "dataio/DataIO.h"
#include "util/ByteBuffer.h"
#include "util/String.h"

namespace muscle {

/** An enumeration of algorithms the IncrementalHashCalculator class supports */
enum {
   HASH_ALGORITHM_MD5 = 0,  ///< MD5 algorithm
   HASH_ALGORITHM_SHA1,     ///< SHA-1 algorithm
   NUM_HASH_ALGORITHMS      ///< guard value
};

/** This object represents the result of an MD5 or SHA1 hash calculation.
  * Note that for MD5 calculations, the final 4 bytes of this value are always zero.
  */
class MUSCLE_NODISCARD IncrementalHash : public PseudoFlattenable<IncrementalHash>
{
public:
   /** Default constructor.  Sets all hash bytes to 0xFF. */
   IncrementalHash() {Reset();}

   /** Explicit Constructor
     * @param hashBytes pointer to some bytes of hash-result data to copy in to this object's internal array
     * @param numBytes the number of bytes that (hashBytes) points to.  If less than MAX_HASH_RESULT_SIZE_BYTES, then
     *                 the remaining bytes in this object will be set to zero.  If more than MAX_HASH_RESULT_SIZE_BYTES,
     *                 the "extra" bytes will be ignored.  Default value is MAX_HASH_RESULT_SIZE_BYTES.
     */
   IncrementalHash(const uint8 * hashBytes, uint32 numBytes=MAX_HASH_RESULT_SIZE_BYTES)
   {
      memcpy(_hashBytes, hashBytes, muscleMin(numBytes, ARRAYITEMS(_hashBytes)));
      for (uint32 i=numBytes; i<MAX_HASH_RESULT_SIZE_BYTES; i++) _hashBytes[i] = 0x00;
   }

   /** For debugging.  Returns the contents of this object as a human-readable hexadecimal string */
   String ToString() const;

   bool operator==(const IncrementalHash & rhs) const {return (memcmp(_hashBytes, rhs._hashBytes, ARRAYITEMS(_hashBytes))==0);}
   bool operator!=(const IncrementalHash & rhs) const {return (memcmp(_hashBytes, rhs._hashBytes, ARRAYITEMS(_hashBytes))!=0);}
   bool operator>( const IncrementalHash & rhs) const {return (memcmp(_hashBytes, rhs._hashBytes, ARRAYITEMS(_hashBytes))>0);}
   bool operator<( const IncrementalHash & rhs) const {return (memcmp(_hashBytes, rhs._hashBytes, ARRAYITEMS(_hashBytes))<0);}

   /** Returns true iff this IncrementalHash has the value set by the default-constructor (all bytes are 0xFF) */
   MUSCLE_NODISCARD bool IsDefaultValue() const
   {
      for (uint32 i=0; i<ARRAYITEMS(_hashBytes); i++) if (_hashBytes[i] != 0xFF) return false;
      return true;
   }

   /** Resets our state back to the just-default-constructed state (ie all bytes are 0xFF) */
   void Reset() {memset(_hashBytes, 0xFF, sizeof(_hashBytes));}

   /** This is here to allow us to use IncrementalHash objects as keys in a Hashtable.
     * The value returned by this method is a hash of our MD5/SHA-1 hash!
     */
   MUSCLE_NODISCARD uint32 HashCode() const {return CalculateHashCode(_hashBytes, sizeof(_hashBytes));}

   /** Same as above, but returns a 64-bit hash value */
   MUSCLE_NODISCARD uint64 HashCode64() const {return CalculateHashCode64(_hashBytes, sizeof(_hashBytes));}

   /** Returns a read-only pointer to the hash-bytes this object contains */
   MUSCLE_NODISCARD const uint8 * GetBytes() const {return _hashBytes;}

   /** The maximum number of bytes in a IncrementalHash */
   enum {MAX_HASH_RESULT_SIZE_BYTES=20};

   // PsuedoFlattenable API
   MUSCLE_NODISCARD static bool IsFixedSize() {return true;}
   MUSCLE_NODISCARD static uint32 TypeCode() {return 1668048993;} // 'clha'
   MUSCLE_NODISCARD static uint32 FlattenedSize() {return MAX_HASH_RESULT_SIZE_BYTES;}
   void Flatten(DataFlattener flat) const {flat.WriteBytes(_hashBytes, sizeof(_hashBytes));}
   status_t Unflatten(DataUnflattener & unflat) {return unflat.ReadBytes(_hashBytes, sizeof(_hashBytes));}

private:
   uint8 _hashBytes[MAX_HASH_RESULT_SIZE_BYTES];  // 20 bytes used for SHA-1, 16 bytes used for MD5
};

/** This is a class that makes it easier to calculate an MD5 or SHA1 hash
  * iteratively, so that you don't need to all have all of the input-data-stream
  * present in memory at once.  (If you do have it all present and in the same
  * byte-array, you can just call the IncrementalHashCalculator::CalculateHashSingleShot()
  * static-method instead)
  */
class MUSCLE_NODISCARD IncrementalHashCalculator : public NotCopyable
{
public:
   /** Constructor
     * @param algorithm a HASH_ALGORITHM_* value indicating which algorithm to use
     */
   IncrementalHashCalculator(uint32 algorithm) : _algorithm(algorithm)
#ifdef MUSCLE_ENABLE_SSL
      , _context(NULL)
      , _tempContext(NULL)
#endif
   {
      Reset();
   }

   ~IncrementalHashCalculator()
   {
#ifdef MUSCLE_ENABLE_SSL
      FreeContext();
#endif
   }

   /** Resets this object back to its just-constructed state */
   void Reset();

   /** Updates our internal-hashing-state to include the specified bytes.
     * @param inBytes pointer to the additional bytes to hash
     * @param numInBytes the number of bytes that (inBytes) points to
     */
   void HashBytes(const uint8 * inBytes, uint32 numInBytes);

   /** Returns the current MD5 or SHA-1 hash code, which is based on all the bytes
     * that were previously passed to AddMoreBytes().
     */
   MUSCLE_NODISCARD IncrementalHash GetCurrentHash() const;

   /** Convenience method:  Computes a one-shot calculation of an MD5 or SHA1 hash for just the passed-in bytes.
     * @param algorithm a HASH_ALGORITHM_* value indicating which algorithm to use
     * @param inBytes Pointer to the bytes to calculate a hash for
     * @param numInBytes The number of bytes pointed to by (inBytes)
     * @note for MD5, only the first 16 bytes of the returned value are valid; the last 4 are always 0.
     */
   MUSCLE_NODISCARD static IncrementalHash CalculateHashSingleShot(uint32 algorithm, const uint8 * inBytes, uint32 numInBytes);

   /** Convenience method:  Reads bytes from (dio) and calculates a hash code.
     * @param algorithm a HASH_ALGORITHM_* value indicating which algorithm to use
     * @param dio the DataIO object to read from.  This object should be in blocking I/O mode.
     * @param maxNumBytesToRead if specified, the maximum number of bytes to read from (dio).  Defaults to (uint64)-1, a.k.a no limit.
     * @note for MD5, only the first 16 bytes of the returned value are valid; the last 4 are always 0.
     */
   MUSCLE_NODISCARD static IncrementalHash CalculateHashSingleShot(uint32 algorithm, DataIO & dio, uint64 maxNumBytesToRead = (uint64)-1);

   /** Convenience method:  Returns the number of hash-result-bytes used by the specified algorithm
     * @param algorithm a HASH_ALGORITHM_* value
     * @returns 16 for MD5, 20 for SHA1, 0 otherwise
     */
   MUSCLE_NODISCARD static uint32 GetNumResultBytesUsedByAlgorithm(uint32 algorithm);

   /** Returns true iff this object is in a usable state.
     * @note if MUSCLE_ENABLE_SSL is not defined, then this method will always return true,
     *       as initialization of our internal hashing mechanisms can never fail.
     */
   bool IsValid() const
   {
#ifdef MUSCLE_ENABLE_SSL
      return (_context != NULL);
#else
      return true;
#endif
   }

private:
   const uint32 _algorithm;  // HASH_ALGORITHM_*

#ifdef MUSCLE_ENABLE_SSL
   void FreeContext();

   // pass-through to OpenSSL's hashing API (may be faster than our native implementation)
   enum {MAX_STATE_SIZE = EVP_MAX_MD_SIZE};
   EVP_MD_CTX * _context;
   EVP_MD_CTX * _tempContext;
#else
   // native implementation (useful when we don't want to depend on the presence of the OpenSSL libraries)
   enum {
      MD5_STATE_SIZE  = 152, // aka sizeof(MD5_CTX)
      SHA1_STATE_SIZE = 248  // aka sizeof(sha1_context)
   };
   template <int S1, int S2> struct _maxx {enum {sz = (S1>S2)?S1:S2};};
   enum {MAX_STATE_SIZE = ((_maxx<MD5_STATE_SIZE, SHA1_STATE_SIZE>::sz)*2)};  // the *2 is just paranoia, in case other CPUs have more padding bytes or something
#endif

   union {
      uint64 _forceAlignment;
      uint8 _stateBytes[MAX_STATE_SIZE];
   } _union;
};

};  // end namespace muscle

#endif
