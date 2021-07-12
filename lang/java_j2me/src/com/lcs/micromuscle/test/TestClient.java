package com.meyer.micromuscle.test;

import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;

import com.meyer.micromuscle.message.Message;
import com.meyer.micromuscle.client.MessageTransceiver;
import com.meyer.micromuscle.client.StorageReflectConstants;
import com.meyer.micromuscle.thread.MessageListener;
import com.meyer.micromuscle.thread.MessageQueue;
import com.meyer.micromuscle.iogateway.AbstractMessageIOGateway;


public class TestClient extends MIDlet implements CommandListener, MessageListener, StorageReflectConstants {
	Display display;
	
	Command cmdExit;
	Command cmdConnect;
	Command cmdDisconnect;
	Command cmdClear;
	Command cmdSend;
	
	TextBox txtIO;
	
	Form frmConnectParams;
	TextField txtHost;
	TextField txtPort;
	
	MessageTransceiver mc;
	
	public TestClient() {
		display = Display.getDisplay(this);
		
		cmdConnect = new Command("Connect", Command.OK, 1);
		cmdDisconnect = new Command("Disconnect", Command.OK, 3);
		cmdExit = new Command("Exit", Command.EXIT, 3);
		cmdClear = new Command("Clear", Command.OK, 1);
		cmdSend = new Command("Send", Command.OK, 2);
		
		txtIO = new TextBox("I/O", "", 512, TextField.ANY);
		txtIO.addCommand(cmdClear);
		txtIO.addCommand(cmdSend);
		txtIO.addCommand(cmdDisconnect);
		txtIO.setCommandListener(this);
		
		frmConnectParams = new Form("Connection Settings");
		txtHost = new TextField("Host:", "30.243.231.39", 18, TextField.ANY);
		txtPort = new TextField("Port:", "2960", 4, TextField.NUMERIC);
		frmConnectParams.append(txtHost);
		frmConnectParams.append(txtPort);
		frmConnectParams.addCommand(cmdConnect);
		frmConnectParams.addCommand(cmdExit);
		frmConnectParams.setCommandListener(this);
		
		MessageQueue mq = new MessageQueue(this);
		mc = new MessageTransceiver(mq, AbstractMessageIOGateway.MUSCLE_MESSAGE_ENCODING_ZLIB_6);
	}
	
	protected void startApp() {
		display.setCurrent(frmConnectParams);
	}
	
	protected void pauseApp() {
	}
	
	protected void destroyApp(boolean unconditional) {
		mc.disconnect();
	}
	
	private void exit() {
		destroyApp(false);
		notifyDestroyed();
	}
	
	public void commandAction(Command c, Displayable d) {
		if (c == cmdExit) {
			exit();
		} else if (c == cmdConnect) {
			connect();
		} else if (c == cmdDisconnect) {
			disconnect();
		} else if (c == cmdClear) {
			txtIO.setString("");
		} else {
			sendCommands(txtIO.getString());
		}
	}
	
	private void connect() {
		mc.connect(txtHost.getString(), Integer.parseInt(txtPort.getString()), "Session Connected", "Session Disconnected");
		display.setCurrent(txtIO);
	}
	
	private void disconnect() {
		mc.disconnect();
	}
	
	public synchronized void messageReceived(Object message, int numLeft) {
		if (message instanceof Message) {
			txtIO.setString(message.toString());
		} else {
			txtIO.setString(message.toString());
			if (message.equals("Session Connected")) {
				mc.sendOutgoingMessage(new Message(PR_COMMAND_GETPARAMETERS));
			} else if (message.equals("Session Disconnected")) {
				display.setCurrent(frmConnectParams);
			}
		}
	}
	
	private void sendCommands(String line) {
		StringTokenizer st = new StringTokenizer(line);
		
		String command = st.nextToken();
		Message msg = new Message(PR_COMMAND_PING);
		if (command.equalsIgnoreCase("q")) {
			exit();
		} else if (command.equalsIgnoreCase("t")) {
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
		} else if (command.equals("s")) {
			msg.what = PR_COMMAND_SETDATA;
			msg.setMessage(st.nextToken(), new Message(1234));
		} else if (command.equals("k")) {
			msg.what = PR_COMMAND_KICK;
			msg.setString(PR_NAME_KEYS, st.nextToken());
		} else if (command.equals("b")) {
			msg.what = PR_COMMAND_ADDBANS;
			msg.setString(PR_NAME_KEYS, st.nextToken());
		} else if (command.equals("B")) {
			msg.what = PR_COMMAND_REMOVEBANS;
			msg.setString(PR_NAME_KEYS, st.nextToken());
		} else if (command.equals("g")) {
			msg.what = PR_COMMAND_GETDATA;
			msg.setString(PR_NAME_KEYS, st.nextToken());
		} else if (command.equals("p")) {
			msg.what = PR_COMMAND_SETPARAMETERS;
			msg.setString(st.nextToken(), "");
		} else if (command.equals("P")) {
			msg.what = PR_COMMAND_GETPARAMETERS;
		} else if (command.equals("d")) {
			msg.what = PR_COMMAND_REMOVEDATA;
			msg.setString(PR_NAME_KEYS, st.nextToken());
		} else if (command.equals("D")) {
			msg.what = PR_COMMAND_REMOVEPARAMETERS;
			msg.setString(PR_NAME_KEYS, st.nextToken());
		} else if (command.equals("big")) {
			msg.what = PR_COMMAND_PING;
			int numFloats = 1*1024*1024;
			float lotsafloats[] = new float[numFloats];  // 4MB worth of floats!
			for (int i=0; i<numFloats; i++) {
				lotsafloats[i] = (float)i;
			}
			msg.setFloats("four_megabytes_of_floats", lotsafloats);                  
		} else if (command.equals("toobig")) {
			msg.what = PR_COMMAND_PING;
			int numFloats = 2*1024*1024;
			float lotsafloats[] = new float[numFloats];  // 8MB worth of floats!
			for (int i=0; i<numFloats; i++) {
				lotsafloats[i] = (float)i;
			}
			msg.setFloats("eight_megabytes_of_floats", lotsafloats);                  
		}
		
		if (msg != null) {
			mc.sendOutgoingMessage(msg);
		}
	}
	
	/**
	 * Super hack-job StringTokenizer! This -should- work, but nothing here is guaranteed!
	 */
	private class StringTokenizer {
		String st;
		int tokLoc;
		
		String defDelims = " \t\n\r\f";
		
		StringTokenizer(String string) {
			st = string;
			tokLoc = 0;
		}
		
		public boolean hasMoreTokens() {
			return tokLoc < st.length();
		}
		
		public String nextToken() {
			return nextToken(defDelims);
		}
		
		public String nextToken(String delims) {
			// IMPLEMENT
			int stStart = tokLoc;
			int stEnd = st.length();
			for (int del = 0; del < delims.length(); del++) {
				char[] delim = new char[1];
				delim[0] = delims.charAt(del);
				int idx = st.indexOf(new String(delim), stStart);
				if (idx != -1 && idx < stEnd) {
					stEnd = idx;
				}
			}
			tokLoc = stEnd + 1;
			return st.substring(stStart, stEnd);
		}
		
		public boolean hasMoreElements() {
			return hasMoreTokens();
		}
		
		public Object nextElement() {
			return nextToken();
		}
		
		public int countTokens() {
			int count = 0;
			// Remember the position
			int oldEnd = tokLoc;
			
			while (hasMoreTokens()) {
				nextToken();
				count++;
			}
			
			// Restore the position
			tokLoc = oldEnd;
			
			return count;
		}
	}
}
