/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.muscle.test;

import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.IOException;

import java.util.StringTokenizer;
import java.util.NoSuchElementException;

import com.meyer.muscle.iogateway.AbstractMessageIOGateway;
import com.meyer.muscle.message.Message;
import com.meyer.muscle.client.MessageTransceiver;
import com.meyer.muscle.client.StorageReflectConstants;
import com.meyer.muscle.support.UnflattenFormatException;
import com.meyer.muscle.thread.MessageListener;
import com.meyer.muscle.thread.MessageQueue;

/** A quick test class, similar to the C++ muscle/test/testreflectclient program */
public class TestClient implements MessageListener, StorageReflectConstants
{
   public static final void main(String args[])
   {
      if (args.length != 1)
      {
         System.out.println("Usage java com.meyer.muscle.test.TestClient 12.18.240.15");
         System.exit(5);
      }
      
      new TestClient(args[0], (args.length >= 2) ? (new Integer(args[1])).intValue() : 2960);
   }
   
   public TestClient(String hostName, int port)
   {      
      // Connect here....
      MessageTransceiver mc = new MessageTransceiver(new MessageQueue(this), AbstractMessageIOGateway.MUSCLE_MESSAGE_ENCODING_ZLIB_6, 6*1024*1024);
      System.out.println("Connecting to " + hostName + ":" + port);
      mc.connect(hostName, port, "Session Connected", "Session Disconnected");
       
      // Listen for user input
      try {
         BufferedReader br = new BufferedReader(new InputStreamReader(System.in));
         String nextLine;
         while((nextLine = br.readLine()) != null)
         {
            System.out.println("You typed: ["+nextLine+"]");  
            StringTokenizer st = new StringTokenizer(nextLine);
            try {
               String command = st.nextToken();
               Message msg = new Message(PR_COMMAND_PING);
               if (command.equalsIgnoreCase("q")) break;
               else if (command.equalsIgnoreCase("t"))
               {
                  int [] ints = {1,3};
                  msg.setInts("ints", ints);
                  float [] floats = {2, 4};
                  msg.setFloats("floats", floats);
                  double [] doubles = {-5, -10};
                  msg.setDoubles("doubles", doubles);
                  byte [] bytes = {0x50};
                  msg.setBytes("bytes", bytes);
                  short [] shorts = {3,15};
                  msg.setShorts("shorts", shorts);
                  long [] longs = {50000};
                  msg.setLongs("longs", longs);
                  boolean [] booleans = {true, false};
                  msg.setBooleans("booleans", booleans);
                  String [] strings = {"Hey", "What's going on?"};
                  msg.setStrings("strings", strings);
                  
                  Message q = (Message) msg.cloneFlat();
                  Message r = new Message(1234);
                  r.setString("I'm way", "down here!");
                  q.setMessage("submessage", r);
                  msg.setMessage("message", q);
               }
               else if (command.equals("s")) {
                  msg.what = PR_COMMAND_SETDATA;
                  msg.setMessage(st.nextToken(), new Message(1234));
               }   
               else if (command.equals("k")) {
                  msg.what = PR_COMMAND_KICK;
                  msg.setString(PR_NAME_KEYS, st.nextToken());
               }
               else if (command.equals("b")) {
                  msg.what = PR_COMMAND_ADDBANS;
                  msg.setString(PR_NAME_KEYS, st.nextToken());
               }
               else if (command.equals("B")) {
                  msg.what = PR_COMMAND_REMOVEBANS;
                  msg.setString(PR_NAME_KEYS, st.nextToken());
               }
               else if (command.equals("g")) {
                  msg.what = PR_COMMAND_GETDATA;
                  msg.setString(PR_NAME_KEYS, st.nextToken());
               }   
               else if (command.equals("p")) {
                  msg.what = PR_COMMAND_SETPARAMETERS;
                  msg.setString(st.nextToken(), "");
               }   
               else if (command.equals("P")) {
                  msg.what = PR_COMMAND_GETPARAMETERS;
               }   
               else if (command.equals("d")) {
                  msg.what = PR_COMMAND_REMOVEDATA;
                  msg.setString(PR_NAME_KEYS, st.nextToken());
               }   
               else if (command.equals("D")) {
                  msg.what = PR_COMMAND_REMOVEPARAMETERS;
                  msg.setString(PR_NAME_KEYS, st.nextToken());
               }              
               else if (command.equals("big")) {
                  msg.what = PR_COMMAND_PING;
                  int numFloats = 1*1024*1024;
                  float lotsafloats[] = new float[numFloats];  // 4MB worth of floats!
                  for (int i=0; i<numFloats; i++) lotsafloats[i] = (float)i;
                  msg.setFloats("four_megabytes_of_floats", lotsafloats);                  
               }
               else if (command.equals("toobig")) {
                  msg.what = PR_COMMAND_PING;
                  int numFloats = 2*1024*1024;
                  float lotsafloats[] = new float[numFloats];  // 8MB worth of floats!
                  for (int i=0; i<numFloats; i++) lotsafloats[i] = (float)i;
                  msg.setFloats("eight_megabytes_of_floats", lotsafloats);                  
               }
               else if (command.equals("e")) {
                   msg.what = PR_COMMAND_SETPARAMETERS;
                   msg.setInt(PR_NAME_REPLY_ENCODING, AbstractMessageIOGateway.MUSCLE_MESSAGE_ENCODING_ZLIB_6);
               }
               if (msg != null) mc.sendOutgoingMessage(msg);
            }
            catch(NoSuchElementException ex) {
               System.out.println("Missing argument!");
            }   
         }
      }
      catch(UnflattenFormatException ex) {
         ex.printStackTrace();
      }
      catch(IOException ex) {
         ex.printStackTrace();
      }
           
      System.out.println("Bye!");
      mc.disconnect();
   }
   
   /** Note that this method will be called from another thread! */
   public synchronized void messageReceived(Object message, int numLeft)
   {
      if (message instanceof Message) System.out.println("Got message: " + message);
      else
      {
         System.out.println("Got non-Message: " + message);
         if (message.equals("Session Disconnected")) System.exit(5);
      }      
   }
}
