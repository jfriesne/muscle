/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleEndianConverter_h
#define MuscleEndianConverter_h

#include "support/MuscleSupport.h"

namespace muscle {

/** This class defines a standardized API for encoding POD data values to little-endian format for serialization, and vice-versa. */
class LittleEndianConverter MUSCLE_FINAL_CLASS
{
public:
///@{
   /** Method for taking a CPU-native POD value and writing it out to a serialized buffer in little-endian format
     * @param readFrom the value to write to the buffer
     * @param writeTo the memory location to write the little-endian data to.  sizeof(readFrom) bytes will be written there.
     *                Note that (writeTo) is NOT required to be an aligned address; these methods will handle unaligned writes correctly.
     */
   static void Export(const   bool & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const   int8 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  uint8 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  int16 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT16(  readFrom));}
   static void Export(const  int32 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT32(  readFrom));}
   static void Export(const  int64 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT64(  readFrom));}
   static void Export(const  float & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_IFLOAT( readFrom));}
   static void Export(const double & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_IDOUBLE(readFrom));}
   static void Export(const uint16 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT16(  readFrom));}
   static void Export(const uint32 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT32(  readFrom));}
   static void Export(const uint64 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT64(  readFrom));}
///@}

///@{
   /** Method for reading from a serialized buffer in little-endian format and using it to set a CPU-native POD value
     * @param readFrom the memory location to read the little-endian data from.  sizeof(val) bytes will be read from there.
     *                 Note that (readFrom) is NOT required to be an aligned address; these methods will handle unaligned reads correctly.
     * @param writeTo the POD-type variable whose value should be set based on the bytes we read.
     */
   static void Import(const void * readFrom,   bool & writeTo) {writeTo = muscleCopyIn< bool>(readFrom);}
   static void Import(const void * readFrom,   int8 & writeTo) {writeTo = muscleCopyIn< int8>(readFrom);}
   static void Import(const void * readFrom,  uint8 & writeTo) {writeTo = muscleCopyIn<uint8>(readFrom);}
   static void Import(const void * readFrom,  int16 & writeTo) {writeTo = B_LENDIAN_TO_HOST_INT16(  muscleCopyIn< int16>(readFrom));}
   static void Import(const void * readFrom,  int32 & writeTo) {writeTo = B_LENDIAN_TO_HOST_INT32(  muscleCopyIn< int32>(readFrom));}
   static void Import(const void * readFrom,  int64 & writeTo) {writeTo = B_LENDIAN_TO_HOST_INT64(  muscleCopyIn< int64>(readFrom));}
   static void Import(const void * readFrom,  float & writeTo) {writeTo = B_LENDIAN_TO_HOST_IFLOAT( muscleCopyIn< int32>(readFrom));}
   static void Import(const void * readFrom, double & writeTo) {writeTo = B_LENDIAN_TO_HOST_IDOUBLE(muscleCopyIn< int64>(readFrom));}
   static void Import(const void * readFrom, uint16 & writeTo) {writeTo = B_LENDIAN_TO_HOST_INT16(  muscleCopyIn<uint16>(readFrom));}
   static void Import(const void * readFrom, uint32 & writeTo) {writeTo = B_LENDIAN_TO_HOST_INT32(  muscleCopyIn<uint32>(readFrom));}
   static void Import(const void * readFrom, uint64 & writeTo) {writeTo = B_LENDIAN_TO_HOST_INT64(  muscleCopyIn<uint64>(readFrom));}
///@}

   /** Convenience method for reading a POD value from a serialized byte-buffer and returning it
     * @tparam T the type of object to return.
     * @param readFrom the memory location to read the little-endian data from.  sizeof(T) bytes will be read from there.
     *        Note that (readFrom) is NOT required to be an aligned address; this method will handle unaligned reads correctly.
     */
   template<typename T> static T Import(const void * readFrom) {T ret; Import(readFrom, ret); return ret;}
};

#if defined(MUSCLE_USE_BIG_ENDIAN_DATA_FOR_EVERYTHING)
typedef    BigEndianConverter DefaultEndianConverter;  /**< MUSCLE uses little-endian encoding as its preferred endian-ness, but this can be changed by defining MUSCLE_USE_*_ENDIAN_DATA_FOR_EVERYTHING. */
#elif defined(DMUSCLE_USE_NATIVE_ENDIAN_DATA_FOR_EVERYTHING)
typedef NativeEndianConverter DefaultEndianConverter;  /**< MUSCLE uses little-endian encoding as its preferred endian-ness, but this can be changed by defining MUSCLE_USE_*_ENDIAN_DATA_FOR_EVERYTHING. */
#else
typedef LittleEndianConverter DefaultEndianConverter;  /**< MUSCLE uses little-endian encoding as its preferred endian-ness, but this can be changed by defining MUSCLE_USE_*_ENDIAN_DATA_FOR_EVERYTHING. */
#endif

/** This class defines a standardized API for encoding POD data values to big-endian format for serialization, and vice-versa. */
class BigEndianConverter MUSCLE_FINAL_CLASS
{
public:
///@{
   /** Method for taking a CPU-native POD value and writing it out to a serialized buffer in big-endian format
     * @param readFrom the POD-type value to write into the buffer
     * @param writeTo the memory location to write the big-endian data to.  sizeof(readFrom) bytes will be written there.
     *                Note that (writeTo) is NOT required to be an aligned address; these methods will handle unaligned writes correctly.
     */
   static void Export(const   bool & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const   int8 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  uint8 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  int16 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT16(  readFrom));}
   static void Export(const  int32 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT32(  readFrom));}
   static void Export(const  int64 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT64(  readFrom));}
   static void Export(const  float & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_IFLOAT( readFrom));}
   static void Export(const double & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_IDOUBLE(readFrom));}
   static void Export(const uint16 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT16(  readFrom));}
   static void Export(const uint32 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT32(  readFrom));}
   static void Export(const uint64 & readFrom, void * writeTo) {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT64(  readFrom));}
///@}

///@{
   /** Method for a serialized buffer in big-endian format and using it to set a CPU-native POD value
     * @param readFrom the memory location to read the big-endian data from.  sizeof(val) bytes will be read from there.
     *                 Note that (readFrom) is NOT required to be an aligned address; these methods will handle unaligned reads correctly.
     * @param writeTo the POD-type variable whose value should be set based on the bytes we read.
     */
   static void Import(const void * readFrom,   bool & writeTo) {writeTo = muscleCopyIn< bool>(readFrom);}
   static void Import(const void * readFrom,   int8 & writeTo) {writeTo = muscleCopyIn< int8>(readFrom);}
   static void Import(const void * readFrom,  uint8 & writeTo) {writeTo = muscleCopyIn<uint8>(readFrom);}
   static void Import(const void * readFrom,  int16 & writeTo) {writeTo = B_BENDIAN_TO_HOST_INT16(  muscleCopyIn< int16>(readFrom));}
   static void Import(const void * readFrom,  int32 & writeTo) {writeTo = B_BENDIAN_TO_HOST_INT32(  muscleCopyIn< int32>(readFrom));}
   static void Import(const void * readFrom,  int64 & writeTo) {writeTo = B_BENDIAN_TO_HOST_INT64(  muscleCopyIn< int64>(readFrom));}
   static void Import(const void * readFrom,  float & writeTo) {writeTo = B_BENDIAN_TO_HOST_IFLOAT( muscleCopyIn< int32>(readFrom));}
   static void Import(const void * readFrom, double & writeTo) {writeTo = B_BENDIAN_TO_HOST_IDOUBLE(muscleCopyIn< int64>(readFrom));}
   static void Import(const void * readFrom, uint16 & writeTo) {writeTo = B_BENDIAN_TO_HOST_INT16(  muscleCopyIn<uint16>(readFrom));}
   static void Import(const void * readFrom, uint32 & writeTo) {writeTo = B_BENDIAN_TO_HOST_INT32(  muscleCopyIn<uint32>(readFrom));}
   static void Import(const void * readFrom, uint64 & writeTo) {writeTo = B_BENDIAN_TO_HOST_INT64(  muscleCopyIn<uint64>(readFrom));}
///@}

   /** Convenience method for reading a POD value from a serialized byte-buffer and returning it
     * @tparam T the type of object to return.
     * @param readFrom the memory location to read the little-endian data from.  sizeof(T) bytes will be read from there.
     *        Note that (readFrom) is NOT required to be an aligned address; this method will handle unaligned reads correctly.
     */
   template<typename T> static T Import(const void * readFrom) {T ret; Import(readFrom, ret); return ret;}
};

/** This class defines a standardized API for encoding POD data values to native-endian format for serialization, and vice-versa.
  * That conversion isn't quite a no-op, since even with native-endian encoding we still have to correctly handle unaligned-pointers, but it's close.
  */
class NativeEndianConverter MUSCLE_FINAL_CLASS
{
public:
///@{
   /** Method for taking a CPU-native POD value and writing it out to a serialized buffer in big-endian format
     * @param readFrom the value to write to the buffer
     * @param writeTo the memory location to write the big-endian data to.  sizeof(readFrom) bytes will be written there.
     *                Note that (writeTo) is NOT required to be an aligned address; these methods will handle unaligned writes correctly.
     */
   static void Export(const   bool & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const   int8 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  uint8 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  int16 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  int32 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  int64 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const  float & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const double & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const uint16 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const uint32 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
   static void Export(const uint64 & readFrom, void * writeTo) {muscleCopyOut(writeTo, readFrom);}
///@}

///@{
   /** Method for a serialized buffer in big-endian format and using it to set a CPU-native POD value
     * @param readFrom the memory location to read the big-endian data from.  sizeof(readFrom) bytes will be read from there.
     *                 Note that (readFrom) is NOT required to be an aligned address; these methods will handle unaligned reads correctly.
     * @param writeTo the POD-type variable whose value should be set based on the bytes we read.
     */
   static void Import(const void * readFrom,   bool & writeTo) {writeTo = muscleCopyIn<  bool>(readFrom);}
   static void Import(const void * readFrom,   int8 & writeTo) {writeTo = muscleCopyIn<  int8>(readFrom);}
   static void Import(const void * readFrom,  uint8 & writeTo) {writeTo = muscleCopyIn< uint8>(readFrom);}
   static void Import(const void * readFrom,  int16 & writeTo) {writeTo = muscleCopyIn< int16>(readFrom);}
   static void Import(const void * readFrom,  int32 & writeTo) {writeTo = muscleCopyIn< int32>(readFrom);}
   static void Import(const void * readFrom,  int64 & writeTo) {writeTo = muscleCopyIn< int64>(readFrom);}
   static void Import(const void * readFrom,  float & writeTo) {writeTo = muscleCopyIn< float>(readFrom);}
   static void Import(const void * readFrom, double & writeTo) {writeTo = muscleCopyIn<double>(readFrom);}
   static void Import(const void * readFrom, uint16 & writeTo) {writeTo = muscleCopyIn<uint16>(readFrom);}
   static void Import(const void * readFrom, uint32 & writeTo) {writeTo = muscleCopyIn<uint32>(readFrom);}
   static void Import(const void * readFrom, uint64 & writeTo) {writeTo = muscleCopyIn<uint64>(readFrom);}
///@}

   /** Convenience method for reading a POD value from a serialized byte-buffer and returning it
     * @tparam T the type of object to return.
     * @param readFrom the memory location to read the little-endian data from.  sizeof(T) bytes will be read from there.
     *        Note that (readFrom) is NOT required to be an aligned address; this method will handle unaligned reads correctly.
     */
   template<typename T> static T Import(const void * readFrom) {T ret; Import(readFrom, ret); return ret;}
};

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

/** This class is an implementation detail for the DataUnflattener class; you can ignore it */
class RealSizeChecker
{
public:
   static bool IsSizeOkay(uint32 numBytes, uint32 numBytesAvailable) {return (numBytes <= numBytesAvailable);}
};

/** This class is an implementation detail for the DataUnflattener class; you can ignore it */
class DummySizeChecker
{
public:
   static bool IsSizeOkay(uint32 /*numBytes*/, uint32 /*numBytesAvailable*/) {return true;}
};

#endif

} // end namespace muscle

#endif
