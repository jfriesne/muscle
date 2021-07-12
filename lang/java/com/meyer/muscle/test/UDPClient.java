package com.meyer.muscle.test;

import java.io.IOException;
import java.net.InetAddress;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.MulticastSocket;

import com.meyer.muscle.iogateway.AbstractMessageIOGateway;
import com.meyer.muscle.iogateway.MessageIOGatewayFactory;
import com.meyer.muscle.message.Message;
import com.meyer.muscle.client.DatagramPacketTransceiver;
import com.meyer.muscle.thread.MessageListener;
import com.meyer.muscle.thread.MessageQueue;

public class UDPClient extends DatagramPacketTransceiver {
	MessageQueue sendQueue;
	MessageQueue receiveQueue;
	
	public UDPClient() throws Exception {
// Using a slave gateway
//		super(new DatagramSocket(1234, InetAddress.getLocalHost()), null, MessageIOGatewayFactory.getMessageIOGateway(AbstractMessageIOGateway.MUSCLE_MESSAGE_ENCODING_ZLIB_9));
// Raw Message I/O
		super(new DatagramSocket(1234, InetAddress.getLocalHost()));
		sendQueue = new MessageQueue(this);
		
		receiveQueue = new MessageQueue(new MessageListener() {
			public synchronized void messageReceived(Object message, int numLeftInQueue) throws Exception {
				System.out.println("Received: " + message);
			}
		});
		setListenQueue(receiveQueue);
	}
	
	public DatagramPacket prepPacket(DatagramPacket packet) {
		try {
			packet.setAddress(InetAddress.getLocalHost());
		} catch (Exception ex) {
		}
		packet.setPort(1234);
		return packet;
	}
	
	public static void main(String[] args) {
		try {
			UDPClient foo = new UDPClient();
			foo.start();
			foo.sendQueue.postMessage(new Message(1097756239));
			//foo.shutdown();
		} catch (Exception ex) {
		}
	}
}
