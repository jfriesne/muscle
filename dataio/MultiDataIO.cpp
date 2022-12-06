#include "dataio/MultiDataIO.h"

namespace muscle {

io_status_t MultiDataIO :: Read(void * buffer, uint32 size)
{
   if (HasChildren())
   {
      const io_status_t ret = GetFirstChild()->Read(buffer, size);
      if (ret.IsError())
      {
         if ((_absorbPartialErrors)&&(_childIOs.GetNumItems() > 1))
         {
            (void) _childIOs.RemoveHead();
            return Read(buffer, size);  // try again with the new first child
         }
         else return ret;
      }
      else if (ret.GetByteCount() > 0)
      {
         MRETURN_ON_ERROR(SeekAll(1, ret.GetByteCount(), IO_SEEK_CUR));
         return ret;
      }
   }
   return io_status_t();
}

io_status_t MultiDataIO :: Write(const void * buffer, uint32 size)
{
   int64 newSeekPos       = -1;  // just to shut the compiler up
   uint32 maxWrittenBytes = 0;
   uint32 minWrittenBytes = MUSCLE_NO_LIMIT;
   for (int32 i=_childIOs.GetNumItems()-1; i>=0; i--)
   {
      const io_status_t childRet = _childIOs[i]()->Write(buffer, muscleMin(size, minWrittenBytes));
      if (childRet.IsError())
      {
         if ((_absorbPartialErrors)&&(_childIOs.GetNumItems() > 1)) (void) _childIOs.RemoveItemAt(i);
                                                               else return childRet;
      }
      else
      {
         if ((uint32)childRet.GetByteCount() < minWrittenBytes)
         {
            minWrittenBytes = childRet.GetByteCount();

            SeekableDataIO * sdio = dynamic_cast<SeekableDataIO *>(_childIOs[i]());
            newSeekPos = sdio ? sdio->GetPosition() : -1;
         }
         maxWrittenBytes = muscleMax(maxWrittenBytes, (uint32)childRet.GetByteCount());
      }
   }

   if (minWrittenBytes < maxWrittenBytes)
   {
      // Oh dear, some children wrote more bytes than others.  To make their seek-positions equal again,
      // we are going to seek everybody to the seek-position of the child that wrote the fewest bytes.
      MRETURN_ON_ERROR(SeekAll(0, newSeekPos, IO_SEEK_CUR));
   }

   return io_status_t((maxWrittenBytes > 0) ? minWrittenBytes : 0);  // the conditional is there in case minWrittenBytes is still MUSCLE_NO_LIMIT
}

void MultiDataIO :: FlushOutput()
{
   for (int32 i=_childIOs.GetNumItems()-1; i>=0; i--) _childIOs[i]()->FlushOutput();
}

void MultiDataIO :: WriteBufferedOutput()
{
   for (int32 i=_childIOs.GetNumItems()-1; i>=0; i--) _childIOs[i]()->WriteBufferedOutput();
}

bool MultiDataIO :: HasBufferedOutput() const
{
   for (int32 i=_childIOs.GetNumItems()-1; i>=0; i--) if (_childIOs[i]()->HasBufferedOutput()) return true;
   return false;
}

status_t MultiDataIO :: SeekAll(uint32 first, int64 offset, int whence)
{
   status_t ret;
   for (int32 i=_childIOs.GetNumItems()-1; i>=(int32)first; i--)
   {
      SeekableDataIO * sdio = dynamic_cast<SeekableDataIO *>(_childIOs[i]());
      if ((sdio == NULL)||(sdio->Seek(offset, whence).IsError(ret)))
      {
         if ((_absorbPartialErrors)&&(_childIOs.GetNumItems() > 1)) (void) _childIOs.RemoveItemAt(i);
                                                               else return ret | B_ERROR;
      }
   }
   return B_NO_ERROR;
}

} // end namespace muscle
