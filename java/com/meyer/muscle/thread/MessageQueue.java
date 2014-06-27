/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.muscle.thread;

import com.meyer.muscle.queue.Queue;

/** This object represents a "thread" of message processing.
 *  Anyone may post a message to it at any time, and it will
 *  queue up incoming messages and dole them out to its listeners,
 *  asynchronously but in order.  Each listener is guaranteed 
 *  to see the message objects in the order they were posted to 
 *  the MessageQueue, but when there are multiple MessageListeners, 
 *  their messageReceived() callbacks will be called in parallel.
 */
public class MessageQueue implements MessageListener, Runnable
{
   /** Creates a MessageQueue with no MessageListeners attached.
    *  Call addListener() to add some.
    */
   public MessageQueue() {/* empty */}

   /** Creates a MessageQueue with a single MessageListener attached.
    *  @param ml The listener to add.
    *  You can call addListener() to add more.
    */
   public MessageQueue(MessageListener ml) {_listener = ml;}

   /** Post a new message object to the queue.  It will be handled asynchronously.
    *  @param message Any Object you care to pass in.  May be null.
    */
   public synchronized void postMessage(Object message)
   {
      _messageQueue.appendElement(message);

      // start the processing thread if it isn't already going
      if (_threadActive == false)
      {      
         _threadActive = true;
         ((_threadPool != null) ? _threadPool : ThreadPool.getDefaultThreadPool()).startThread(this);
      }
   }

   /** Calls interrupt() on the running Thread, or if the Thread is active but not
     * running yet, schedules an interrupt to be called.  Has no effect if there is
     * no Thread active or scheduled.
     */
   public synchronized void interrupt()
   {
      if (_threadActive)
      {
         if (_myThread != null) _myThread.interrupt();  // okay, we can do it right away
                           else _interruptPending = true;
      }
   }

   /** Add a listener to our listeners list 
    *  Note:  Done asynchronously.
    */
   public void addListener(MessageListener listener) {postMessage(new ListControlMessage(listener, true));}

   /** Remove a listener from our listeners list 
    *  Note:  Done asynchronously.
    */
   public void removeListener(MessageListener listener) {postMessage(new ListControlMessage(listener, false));}

   /** Remove all listeners from our listeners list. 
    *  Note:  Done asynchronously.
    */
   public void clearListeners(MessageListener listener) {postMessage(new ListControlMessage(null, false));}

   /** Called on all attached MessageListeners whenever a MessageQueue receives a message. 
    *  This implementation simply calls postMessage(message).  It is here so you can
    *  chain MessageQueues together by adding them as MessageListeners to one another.
    *  @param message The received message object (may be any Java class).
    *  @param numLeftInQueue The number of objects still in the MessageQueue after this one.
    */
   public void messageReceived(Object message, int numLeftInQueue) {postMessage(message);}

   /** By default, the MessageQueue uses the singleton ThreadPool provided by the
    *  ThreadPool class.  If you want it to use a different one, you can specify that here.
    *  @param tp The ThreadPool to get Thread objects from, or null if you wish to use the default again.
    */
   public synchronized void setThreadPool(ThreadPool tp) {_threadPool = tp;}
   
   /** Implementation detail, please ignore this. */ 
   public void run()
   {
      synchronized(this)
      {
         _myThread = Thread.currentThread();

         if (_interruptPending)
         {
            _interruptPending = false;

            // schedule another Thread to interrupt() us ASAP....
            ((_threadPool != null) ? _threadPool : ThreadPool.getDefaultThreadPool()).startThread(new ThreadInterruptor(_myThread));
         }
      }

      while(true)
      {
         Object nextMessage;
         int messagesLeft;
 
         // Grab the next message out of the message queue
         synchronized(this) 
         {
            if (_messageQueue.size() > 0)
            {
               nextMessage  = _messageQueue.removeFirstElement();
               messagesLeft = _messageQueue.size();  
            }
            else 
            {
               _threadActive = false;
               _myThread = null;
               break;   // bye bye!
            }
         }
     
         // Process the message (unsynchronized)
         if (nextMessage instanceof ListControlMessage)
         {
            ListControlMessage lcm = (ListControlMessage) nextMessage;
            if (lcm._add) 
            {
               if (_listeners != null) _listeners.appendElement(new MessageQueue(lcm._listener));
               else  
               {
                  if (_listener != null) 
                  {
                     // convert to multi-listener (asynchronous) mode
                     _listeners = new Queue(); 
                     _listeners.appendElement(new MessageQueue(_listener));
                     _listeners.appendElement(new MessageQueue(lcm._listener));
                     _listener = null;
                  }
                  else _listener = lcm._listener;
               }
            }
            else
            {
               if (lcm._listener != null) 
               {
                  if (_listeners != null)
                  {
                     for (int i=_listeners.size()-1; i>=0; i--)
                     {
                        if (((MessageQueue)_listeners.elementAt(i))._listener == lcm._listener)
                        {
                           _listeners.removeElementAt(i);
                           if (_listeners.size() == 1)
                           {
                              // revert back to single-listener (synchronous) mode
                              _listener  = (MessageListener) _listeners.firstElement();
                              _listeners = null;
                           }
                           break;
                        }
                     }
                  }
                  else if (_listener == lcm._listener) _listener = null;
               }
               else 
               {   
                  _listeners = null;
                  _listener  = null;
               }
            }
         }
         else
         {
            if (_listener != null) 
            {
               try {
                  _listener.messageReceived(nextMessage, messagesLeft);
               }
               catch(Exception ex) {
                  ex.printStackTrace();
               }
            }
            else if (_listeners != null) 
            {
               for (int i=_listeners.size()-1; i>=0; i--) 
               {
                  try {
                     ((MessageQueue)_listeners.elementAt(i)).messageReceived(nextMessage, messagesLeft);
                  }
                  catch(Exception ex) {
                     ex.printStackTrace();
                  }
               }
            }
         }
      }
   }

   /** This class is used to remotely interrupt() my Thread if it turns out to need interrupting
     * (i.e. if an interrupt was requested before the Thread was started)
     * This way the interrupt always affects the internal thread the same way, no matter what.
     */
   private class ThreadInterruptor implements Runnable
   {
      public ThreadInterruptor(Thread thread) {_thread = thread;}
      public void run() {_thread.interrupt();}
   
      private Thread _thread;
   }

   /** Message class used to add or remove listeners */
   private class ListControlMessage 
   {
      public ListControlMessage(MessageListener l, boolean add)
      {
         _listener = l;
         _add = add;
      }

      public MessageListener _listener;
      public boolean _add;
   }
   
   private Queue _messageQueue = new Queue();      // shared (synchronized this)
   private MessageListener _listener = null;       // used if we have exactly one listener
   private Queue _listeners = null;                // used if we have more than one listener
   private boolean _threadActive = false;          // shared (synchronized this)
   private boolean _interruptPending = false;      // shared (synchronized this)
   private Thread _myThread = null;                // set inside of run() (synchronized this)
   private ThreadPool _threadPool = null;          // if a non-default thread pool is to be used, it is set here
}

