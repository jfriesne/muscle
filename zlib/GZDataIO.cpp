/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

#include "zlib/GZDataIO.h"

namespace muscle {

GZDataIO :: GZDataIO(const char * filePath, const char * mode)
   : _file(gzopen(filePath, mode))
{
   // empty
}

GZDataIO :: ~GZDataIO()
{
   ShutdownAux();
}

int32 GZDataIO :: Read(void * buffer, uint32 size)
{
   if (_file == NULL) return -1;
   return gzread(_file, buffer, size);
}

int32 GZDataIO :: Write(const void * buffer, uint32 size)
{
   if (_file == NULL) return -1;
   return gzwrite(_file, buffer, size);
}

void GZDataIO :: FlushOutput()
{
   if (_file) (void) gzflush(_file, Z_BLOCK);
}

void GZDataIO :: Shutdown()
{
   ShutdownAux();
}

void GZDataIO :: ShutdownAux()
{
   if (_file)
   {
      gzclose(_file);
      _file = NULL;
   }
}

const ConstSocketRef & GZDataIO :: GetReadSelectSocket() const
{
   return GetDefaultObjectForType<ConstSocketRef>();
}

const ConstSocketRef & GZDataIO :: GetWriteSelectSocket() const
{
   return GetDefaultObjectForType<ConstSocketRef>();
}

} // end namespace muscle

#endif
