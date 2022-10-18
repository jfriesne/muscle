/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleEndianEncoder_h
#define MuscleEndianEncoder_h

#include "support/MuscleSupport.h"

namespace muscle {

/** This class defines a standardized API for encoding POD data values to little-endian format for serialization, and vice-versa. */
class LittleEndianEncoder
{
public:
   /** Constructor */
   LittleEndianEncoder() {/* empty */};

   /** Destructor */
   ~LittleEndianEncoder() {/* empty */}

///@{
   /** Method for taking a CPU-native POD value and writing it out to a serialized buffer in little-endian format
     * @param val the value to write to the buffer
     * @param writeTo the memory location to write the little-endian data to.  sizeof(val) bytes will be written there.
     *                Note that (writeTo) is NOT required to be an aligned address; these methods will handle unaligned writes correctly.
     */
   void ExportInt16(  int16 val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT16(val));}
   void ExportInt32(  int32 val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT32(val));}
   void ExportInt64(  int64 val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_INT64(val));}
   void ExportFloat(  float val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_IFLOAT(val));}
   void ExportDouble(double val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_LENDIAN_IDOUBLE(val));}
///@}

///@{
   /** Method for a serialized buffer in little-endian format and using it to set a CPU-native POD value
     * @param readFrom the memory location to read the little-endian data from.  sizeof(val) bytes will be read from there.
     *                 Note that (readFrom) is NOT required to be an aligned address; these methods will handle unaligned reads correctly.
     * @returns the corresponding CPU-native POD value.
     */
   int16  ImportInt16( const void * readFrom) const {return B_LENDIAN_TO_HOST_INT16(muscleCopyIn<int16>(readFrom));}
   int32  ImportInt32( const void * readFrom) const {return B_LENDIAN_TO_HOST_INT32(muscleCopyIn<int32>(readFrom));}
   int64  ImportInt64( const void * readFrom) const {return B_LENDIAN_TO_HOST_INT64(muscleCopyIn<int64>(readFrom));}
   float  ImportFloat( const void * readFrom) const {return B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(readFrom));}
   double ImportDouble(const void * readFrom) const {return B_LENDIAN_TO_HOST_IDOUBLE(muscleCopyIn<int64>(readFrom));}
///@}
};

/** This class defines a standardized API for encoding POD data values to big-endian format for serialization, and vice-versa. */
class BigEndianEncoder : public NotCopyable
{
public:
   /** Constructor */
   BigEndianEncoder() {/* empty */};

   /** Destructor */
   ~BigEndianEncoder() {/* empty */}

///@{
   /** Method for taking a CPU-native POD value and writing it out to a serialized buffer in big-endian format
     * @param val the value to write to the buffer
     * @param writeTo the memory location to write the big-endian data to.  sizeof(val) bytes will be written there.
     *                Note that (writeTo) is NOT required to be an aligned address; these methods will handle unaligned writes correctly.
     */
   void ExportInt16(  int16 val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT16(val));}
   void ExportInt32(  int32 val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT32(val));}
   void ExportInt64(  int64 val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_INT64(val));}
   void ExportFloat(  float val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_IFLOAT(val));}
   void ExportDouble(double val, void * writeTo) const {muscleCopyOut(writeTo, B_HOST_TO_BENDIAN_IDOUBLE(val));}
///@}

///@{
   /** Method for a serialized buffer in big-endian format and using it to set a CPU-native POD value
     * @param readFrom the memory location to read the big-endian data from.  sizeof(val) bytes will be read from there.
     *                 Note that (readFrom) is NOT required to be an aligned address; these methods will handle unaligned reads correctly.
     * @returns the corresponding CPU-native POD value.
     */
   int16  ImportInt16( const void * readFrom) const {return B_BENDIAN_TO_HOST_INT16(muscleCopyIn<int16>(readFrom));}
   int32  ImportInt32( const void * readFrom) const {return B_BENDIAN_TO_HOST_INT32(muscleCopyIn<int32>(readFrom));}
   int64  ImportInt64( const void * readFrom) const {return B_BENDIAN_TO_HOST_INT64(muscleCopyIn<int64>(readFrom));}
   float  ImportFloat( const void * readFrom) const {return B_BENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(readFrom));}
   double ImportDouble(const void * readFrom) const {return B_BENDIAN_TO_HOST_IDOUBLE(muscleCopyIn<int64>(readFrom));}
///@}
};

/** This class defines a standardized API for encoding POD data values to native-endian format for serialization, and vice-versa.
  * That conversion isn't quite a no-op, since we still have to handle pointer-alignment issues, but it's close.
  */
class NativeEndianEncoder : public NotCopyable
{
public:
   /** Constructor */
   NativeEndianEncoder() {/* empty */};

   /** Destructor */
   ~NativeEndianEncoder() {/* empty */}

///@{
   /** Method for taking a CPU-native POD value and writing it out to a serialized buffer in big-endian format
     * @param val the value to write to the buffer
     * @param writeTo the memory location to write the big-endian data to.  sizeof(val) bytes will be written there.
     *                Note that (writeTo) is NOT required to be an aligned address; these methods will handle unaligned writes correctly.
     */
   void ExportInt16(  int16 val, void * writeTo) const {muscleCopyOut(writeTo, val);}
   void ExportInt32(  int32 val, void * writeTo) const {muscleCopyOut(writeTo, val);}
   void ExportInt64(  int64 val, void * writeTo) const {muscleCopyOut(writeTo, val);}
   void ExportFloat(  float val, void * writeTo) const {muscleCopyOut(writeTo, val);}
   void ExportDouble(double val, void * writeTo) const {muscleCopyOut(writeTo, val);}
///@}

///@{
   /** Method for a serialized buffer in big-endian format and using it to set a CPU-native POD value
     * @param readFrom the memory location to read the big-endian data from.  sizeof(val) bytes will be read from there.
     *                 Note that (readFrom) is NOT required to be an aligned address; these methods will handle unaligned reads correctly.
     * @returns the corresponding CPU-native POD value.
     */
   int16  ImportInt16( const void * readFrom) const {return muscleCopyIn<int16>(readFrom);}
   int32  ImportInt32( const void * readFrom) const {return muscleCopyIn<int32>(readFrom);}
   int64  ImportInt64( const void * readFrom) const {return muscleCopyIn<int64>(readFrom);}
   float  ImportFloat( const void * readFrom) const {return muscleCopyIn<float>(readFrom);}
   double ImportDouble(const void * readFrom) const {return muscleCopyIn<double>(readFrom);}
///@}
};

} // end namespace muscle

#endif
