/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.thread;

/** Any object that implements this interface may add itself as
 *  a listener to one or more MessageQueue objects.
 */
public interface MessageListener
{
   /** Called on all attached MessageListeners whenever a MessageQueue receives a message.
    *  Note that this method will be called asynchronously from another Thread, so you
    *  may want to declare it synchronized when you implement it.
    *  @param message The received message object (may be any sort of Java object, or even null).
    *  @param numLeftInQueue The number of objects still in the MessageQueue after this one.
    */
   public void messageReceived(Object message, int numLeftInQueue) throws Exception;
}

