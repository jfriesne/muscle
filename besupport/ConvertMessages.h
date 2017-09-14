/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleConvertMessages_h
#define MuscleConvertMessages_h

#include <app/Message.h>
#include "message/Message.h"

namespace muscle {

/*
 *   Functions to convert BMessages to Messages and back.
 *   This code will only compile under BeOS!
 */

/** Converts a Message into a BMessage.  Only compiles under BeOS! 
 *  @param from the Message you wish to convert
 *  @param to the BMessage to write the result into
 *  @return B_NO_ERROR if the conversion succeeded, or B_ERROR if it failed.
 */
status_t ConvertToBMessage(const Message & from, BMessage & to);

/** Converts a BMessage into a Message.  Only compiles under BeOS! 
 *  @param from the BMessage you wish to convert
 *  @param to the Message to write the result into
 *  @return B_NO_ERROR if the conversion succeeded, or B_ERROR if it failed.
 */
status_t ConvertFromBMessage(const BMessage & from, Message & to);

} // end namespace muscle

#endif
