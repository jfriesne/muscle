/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.thread;

import com.meyer.micromuscle.queue.Queue;

/** A ThreadPool is an object that holds a number of threads
 *  and lets you re-use them.  This lets us avoid having
 *  to terminate and re-spawn threads all the time.  This
 *  class is used a lot by the MessageQueue class.
 */
public final class ThreadPool implements Runnable
{
   /** Creates a ThreadPool with a low-water mark of 2 threads,
     * and a high-water mark of 5 threads
     */
   public ThreadPool() {this(2, 5);}

   /** Create a new ThreadPool
    *  @param lowMark Threads will be retained until this many are present.
    *  @param highMark No more than this many threads will be allocated at once.
    */
   public ThreadPool(int lowMark, int highMark)
   {
      _lowMark = lowMark;
      _highMark = highMark;
   }

   /** Schedules a task for execution.  The task may begin
     * right away, or wait until there is a thread available.
     */
   public synchronized void startThread(Runnable target)
   {      
      _tasks.appendElement(target);
      if ((_idleThreadCount <= 0)&&(_threadCount < _highMark))
      {
         _threadCount++;              // note that the new thread exists
         Thread t = new Thread(this);
         t.start();  // and off he goes
      }
      notify();  // wake up a thread to handle our request
   }

   /** A singleton ThreadPool, for convenience */
   public static ThreadPool getDefaultThreadPool() {return _defaultThreadPool;}

   /** Change the singleton ThreadPool if you want */
   public static void setDefaultThreadPool(ThreadPool p) {_defaultThreadPool = p;}

   /** Exposed as an implementation detail.  Please ignore. */
   public void run()
   {
      boolean keepGoing = true;
      
      while(keepGoing)
      {
         // Keep running tasks as long as any are available
         while(true)
         {
            Runnable nextTask = null;

            // Critical section:  Get the next task, if any
            synchronized(this)
            {
               if (_tasks.size() > 0) nextTask = (Runnable) _tasks.removeFirstElement();
               else
               {
                  if (_threadCount > _lowMark)
                  {
                     _threadCount--;
                     keepGoing = false;   // this thread is going away -- there are too many threads!
                  }
               }
            }

            // Important that this is done in an unsynchronized section, to avoid locking the pool
            if (nextTask != null) 
            {
               try {
                  nextTask.run();  // do it!
               }
               catch(Exception e) {
                  e.printStackTrace();
               }
            }
            else break;  // go back to sleep until more tasks are available
         }
 
         if (keepGoing)
         {
            // No more tasks left:  go to sleep
            synchronized(this)
            {
               _idleThreadCount++;  // this thread is idle again
               try {
                  wait();   // wait until startThread() is called again.
               }
               catch(InterruptedException ex) {
                   ex.printStackTrace();  // shouldn't ever happen
               }
               _idleThreadCount--;  // now it's busy
            }
         }
      }
   }

   private static ThreadPool _defaultThreadPool = new ThreadPool();  // singleton

   private Queue _tasks = new Queue();  // Runnables that need to be run
   private int _idleThreadCount = 0;    // how many threads are idle
   private int _threadCount = 0;        // how many threads are in the pool
   private int _lowMark;                // minimum # of threads to retain
   private int _highMark;               // maximum # of threads to allocate
}
