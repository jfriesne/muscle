/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <gui/point.h>
#include <gui/rect.h>
#include "atheossupport/ConvertMessages.h"

namespace muscle {

status_t ConvertToAMessage(const Message & from, os::Message & to) 
{
   to.MakeEmpty();
   to.SetCode(from.what);

   uint32 type;
   uint32 count;
   bool fixedSize;

   for (MessageFieldNameIterator it = from.GetFieldNameIterator(B_ANY_TYPE, HTIT_FLAG_NOREGISTER); it.HasData(); it++)
   {
      const String & n = it.GetFieldName();
      if (from.GetInfo(n, &type, &count, &fixedSize) != B_NO_ERROR) return B_ERROR;

      for (uint32 j=0; j<count; j++)
      {
         const void * nextItem;
         uint32 itemSize;
         if (from.FindData(n, type, j, &nextItem, &itemSize) != B_NO_ERROR) return B_ERROR;

         // do any necessary translation from the Muscle data types to Be data types
         switch(type)
         {
            case B_POINT_TYPE:
            {
               const Point * p = static_cast<const Point *>(nextItem);
               os::Point bpoint(p->x(), p->y());
               if (to.AddPoint(n, bpoint) != B_NO_ERROR) return B_ERROR;
            }
            break;

            case B_RECT_TYPE:
            {
               const Rect * r = static_cast<const Rect *>(nextItem);
               os::Rect brect(r->left(), r->top(), r->right(), r->bottom());
               if (to.AddRect(n, brect) != B_NO_ERROR) return B_ERROR;
            }
            break;
 
            case B_MESSAGE_TYPE:
            {
               const MessageRef * msgRef = static_cast<const MessageRef *>(nextItem);
               os::Message amsg;
               if (msgRef->GetItemPointer() == NULL) return B_ERROR;
               if (ConvertToAMessage(*msgRef->GetItemPointer(), amsg) != B_NO_ERROR) return B_ERROR;
               if (to.AddMessage(n, &amsg) != B_NO_ERROR) return B_ERROR;
            }
            break;

            default:
               if (to.AddData(n, type, nextItem, itemSize, fixedSize, count) != B_NO_ERROR) return B_ERROR;
            break;
         }
      }
   }
   return B_NO_ERROR;
}

status_t ConvertFromAMessage(const os::Message & from, Message & to) 
{
   to.Clear();
   to.what = from.GetCode();

   int numNames = from.GetNumNames();
   for (int32 i=0; i<numNames; i++)
   {
      int type;
      int count;
      std::string name = from.GetName(i);
      if (from.GetNameInfo(name.c_str(), &type, &count) == B_NO_ERROR)
      {
         for (int j=0; j<count; j++)
         {
            const void * nextItem;
            size_t itemSize;
            if (from.FindData(name.c_str(), type, &nextItem, &itemSize, j) != B_NO_ERROR) return B_ERROR;

            // do any necessary translation from the AtheOS data types to Muscle data types
            switch(type)
            {
               case os::T_POINT:
               {
                  const os::Point * p = static_cast<const os::Point *>(nextItem);
                  Point pPoint(p->x, p->y);
                  if (to.AddPoint(name.c_str(), pPoint) != B_NO_ERROR) return B_ERROR;
               }
               break;

               case os::T_RECT:
               {
                  const os::Rect * r = static_cast<const os::Rect *>(nextItem);
                  Rect pRect(r->left, r->top, r->right, r->bottom);
                  if (to.AddRect(name.c_str(), pRect) != B_NO_ERROR) return B_ERROR;
               }
               break;

               case os::T_MESSAGE:
               {
                  os::Message amsg;
                  if (amsg.Unflatten(static_cast<const uint8 *>(nextItem)) != B_NO_ERROR) return B_ERROR;

                  Message * newMsg = newnothrow Message;
                  MRETURN_OOM_ON_NULL(newMsg);

                  MessageRef msgRef(newMsg);
                  if (ConvertFromAMessage(amsg, *newMsg) != B_NO_ERROR) return B_ERROR;
                  if (to.AddMessage(name.c_str(), msgRef) != B_NO_ERROR) return B_ERROR;
               }
               break;

               default:
                  if (to.AddData(name.c_str(), type, nextItem, itemSize) != B_NO_ERROR) return B_ERROR;
               break;
            }
         }
      }
   }
   return B_NO_ERROR;
}

} // end namespace muscle
