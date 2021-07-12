package com.meyer.muscle.client;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.net.SocketAddress;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ByteChannel;
import java.util.HashMap;

import com.meyer.muscle.message.Message;
import com.meyer.muscle.support.NotEnoughDataException;
import com.meyer.muscle.support.UnflattenFormatException;
import com.meyer.muscle.thread.MessageQueue;
import com.meyer.muscle.thread.MessageListener;
import com.meyer.muscle.iogateway.AbstractMessageIOGateway;
import com.meyer.muscle.iogateway.MessageIOGateway;



/**
 * <p>DatagramPacketTransceiver - A class for creating Java UDP clients.</p>
 * 
 * <p>This class is compatible with the C++ muscled PacketTunnelIOGateway.
 * See the class UDPClient in the com.meyer.muscle.test package for example of how 
 * to use this class.</p>
 * 
 * <p>This implementation does not yet pack multiple Message objects into a 
 * single DatagramPacket. It will decode multiple messages packed into 
 * DatagramPackets, making it compatible with the C++ class. This implementation
 * does support self-exclusion, miscellaneous data delivery (ByteBuffers are 
 * passed to listeners instead of Message objects) max output sizes (mtu), and 
 * reassembly of message fragments delivered out of order.
 *
 * @author Bryan.Varner
 */
public class DatagramPacketTransceiver extends Thread implements MessageListener {
	public static final int DEFAULT_MAGIC = 1114989680; // 'Budp'
	public static final int FRAGMENT_HEADER_SIZE = 6;
	public static final int FRAGMENT_HEADER_LENGTH = (FRAGMENT_HEADER_SIZE * 4); // 4 bytes per 32bit int.
	
	// Header values.
	// multiply by 4 = byte offsets into the received DatagramPacket buffer
	public static final int HEADER_MAGIC = 0;
	public static final int HEADER_SEXID = 1;
	public static final int HEADER_MESSAGEID = 2;
	public static final int HEADER_OFFSET = 3;
	public static final int HEADER_CHUNK_SIZE = 4;
	public static final int HEADER_TOTAL_SIZE = 5;
	
	AbstractMessageIOGateway slaveGateway;
	MessageQueue listeners;
	
	int mtu;
	int magic;
	
	boolean allowMisc;
	int maxIncomingMessageSize;
	int sexId;
	
	int outputPacketSize;
	int sendMessageIDCounter;
	
	HashMap<SocketAddress, ReceiveState> receiveStates;
	
	protected boolean halt;
	
	protected DatagramSocket socket;
	
	/**
	 * Creates a PacketTunnelIOGateway that defers to slave, with a MTU, and 
	 * magic number for the header. It will receive packets on the socket, 
	 * decode them using the given gateway, and broadcast the results to the 
	 * MessageQueue. Messages sent with this Transceiver will have a mtu of
	 * maxTransferUnit, and a magic number identified by magic.
	 * 
	 * @param socket The DatagramSocket to send / receive on.
	 * @param listeners The MessageQueue to post received data to.
	 * @param slave An AbstractMessageIOGateway to use when encoding / decoding
	 * @param maxTransferUnit The max size of a datagram packet. 
	 *                        For Ethernet, this should be 1500
	 * @param magic The "magic number" identifying the packets being sent.
	 */
	public DatagramPacketTransceiver(DatagramSocket socket, MessageQueue listeners, AbstractMessageIOGateway slave, int maxTransferUnit, int magic) {
		super("DatagramTransceiver");
		this.slaveGateway = slave;
		this.mtu = Math.max(maxTransferUnit, FRAGMENT_HEADER_LENGTH + 1);
		this.magic = magic;
		
		this.outputPacketSize = 0;
		this.sendMessageIDCounter = 0;
		
		this.maxIncomingMessageSize = Integer.MAX_VALUE;
		this.allowMisc = false;
		this.sexId = 0;
		
		this.socket = socket;
		this.listeners = listeners;
		
		this.receiveStates = new HashMap<SocketAddress, ReceiveState>();
		this.halt = false;
	}
	
	public DatagramPacketTransceiver(DatagramSocket socket, MessageQueue listeners, int maxTransferUnit, int magic) {
		this(socket, listeners, null, maxTransferUnit, magic);
	}
	
	
	public DatagramPacketTransceiver(DatagramSocket socket, MessageQueue listeners, AbstractMessageIOGateway slave, int maxTransferUnit) {
		this(socket, listeners, slave, maxTransferUnit, DEFAULT_MAGIC);
	}
	
	public DatagramPacketTransceiver(DatagramSocket socket, MessageQueue listeners, int maxTransferUnit) {
		this(socket, listeners, null, maxTransferUnit, DEFAULT_MAGIC);
	}
	
	
	public DatagramPacketTransceiver(DatagramSocket socket, MessageQueue listeners, AbstractMessageIOGateway slave) {
		this(socket, listeners, slave, 1500, DEFAULT_MAGIC);
	}
	
	public DatagramPacketTransceiver(DatagramSocket socket, MessageQueue listeners) {
		this(socket, listeners, null);
	}
	
	
	public DatagramPacketTransceiver(DatagramSocket socket) {
		this(socket, null);
	}
	
	
	public void setListenQueue(MessageQueue queue) {
		this.listeners = queue;
	}
	
	
	
	public boolean hasSlaveGateway() {
		return slaveGateway != null;
	}
	
	public AbstractMessageIOGateway getSlaveGateway() {
		return slaveGateway;
	}
	
	
	public int getMaxIncomingMessageSize() {
		return this.maxIncomingMessageSize;
	}
	
	public void setMaxIncomingMessageSize(int size) {
		this.maxIncomingMessageSize = size;
	}
	
	public boolean getAllowMiscIncomingData() {
		return this.allowMisc;
	}
	
	public void setAllowMiscIncomingData(boolean b) {
		this.allowMisc = b;
	}
	
	public int getSourceExclusionId() {
		return sexId;
	}
	
	public void setSourceExclusionId(int sexId) {
		this.sexId = sexId;
	}
	
	
	public void shutdown() {
		halt = true;
		if (this.isAlive()) {
			this.interrupt();
		}
	}
	
	/**
	 * Returns weather or not this has been shut down.
	 */
	public boolean isShutdown() {
		return halt;
	}
	
	protected void preRun() {
	}
	
	protected void postRun() {
	}
	
	public void run() {
		preRun();
		
		int maxIncomingMessageSize = Integer.MAX_VALUE;
		if (slaveGateway != null) {
			slaveGateway.getMaximumIncomingMessageSize();
		}
		
		byte[] buf = new byte[mtu];
		DatagramPacket recv = new DatagramPacket(buf, buf.length);
		try {
			socket.setSoTimeout(1000);
		} catch (SocketException se) {
			se.printStackTrace();
		}
		
		while (!halt) {
			try {
				socket.receive(recv);
				
				ByteBuffer buffer = ByteBuffer.wrap(buf);
				buffer.order(ByteOrder.LITTLE_ENDIAN);
				int packetMagic = buffer.getInt();
				
				if (allowMisc && (buffer.capacity() < FRAGMENT_HEADER_SIZE || packetMagic != magic)) {
					// Pass along the ByteBuffer.
					buffer.rewind();
					listeners.postMessage(buffer);
				} else {
					boolean firstTime = true;
					while (buffer.remaining() >= FRAGMENT_HEADER_LENGTH) {
						int[] header = new int[FRAGMENT_HEADER_SIZE];
						
						/* The first time through, we've already read the 
						 * magic number. so assign it, and mark the 
						 * boolean so that we'll read the magic next time.
						 */
						if (firstTime) {
							header[HEADER_MAGIC] = packetMagic; // already read.
							firstTime = false;
						} else {
							header[HEADER_MAGIC] = buffer.getInt();
						}
						/* Read the headers, starting with SEXID. */
						for (int i = HEADER_SEXID; i < header.length; i++) {
							header[i] = buffer.getInt();
						}
						
						/* Headers read. Check magic, sexid, positions & remaining space */
						if ((header[HEADER_MAGIC] == magic) && 
						    ((header[HEADER_SEXID] == 0) || (header[HEADER_SEXID] != sexId)) &&
						    (((buffer.capacity() - buffer.position()) >= header[HEADER_CHUNK_SIZE]) && (header[HEADER_TOTAL_SIZE] <= maxIncomingMessageSize)))
						{
							ReceiveState rs = receiveStates.get(recv.getSocketAddress());
							if (rs == null) {
								if (header[HEADER_OFFSET] == 0) {
									rs = new ReceiveState(header[HEADER_MESSAGEID]);
									receiveStates.put(recv.getSocketAddress(), rs);
									rs.buffer = ByteBuffer.allocate(header[HEADER_TOTAL_SIZE]);
									rs.buffer.order(ByteOrder.LITTLE_ENDIAN);
								}
							}
							
							if ((header[HEADER_OFFSET] == 0) || (header[HEADER_MESSAGEID] != rs.messageId)) {
								// New Message... start receiving it (but only if we are starting at the beginning).
								rs.messageId = header[HEADER_MESSAGEID];
								rs.buffer.clear();
								if (rs.buffer.capacity() != header[HEADER_TOTAL_SIZE]) {
									// Allocate new buffer.
									rs.buffer = ByteBuffer.allocate(header[HEADER_TOTAL_SIZE]);
									rs.buffer.order(ByteOrder.LITTLE_ENDIAN);
								}
							}
							
							int rsCapacity = rs.buffer.capacity();
							if ((header[HEADER_MESSAGEID] == rs.messageId) && 
							    (header[HEADER_TOTAL_SIZE] == rsCapacity) &&
							    (header[HEADER_OFFSET] == rs.buffer.position()) &&
							    (header[HEADER_OFFSET] + header[HEADER_CHUNK_SIZE] <= rsCapacity))
							{
								rs.buffer.put(buffer.array(), buffer.position(), header[HEADER_CHUNK_SIZE]);
								if (rs.buffer.remaining() == 0 && header[HEADER_TOTAL_SIZE] > 0) {
									rs.buffer.clear(); // this doesn't actually clear the backing buffer data!
									try {
										Message m = null;
										if (slaveGateway != null) {
											m = slaveGateway.unflattenMessage(rs.buffer);
										} else {
											m = new Message();
											m.unflatten(rs.buffer, rs.buffer.remaining());
										}
										
										if (listeners != null) {
											listeners.postMessage(m);
										}
									} catch (NotEnoughDataException nede) {
										System.err.println("Missing " + nede.getNumMissingBytes() + " bytes.");
									}
									rs.buffer.clear();
								}
							} else if (header[HEADER_OFFSET] != rs.buffer.position()) {
								System.err.println("specified offset: " + header[HEADER_OFFSET] + " is not current buffer position: " + rs.buffer.position());
							} else {
								rs.buffer.clear();
							}
							buffer.position(buffer.position() + header[HEADER_CHUNK_SIZE]);
						} else {
							break;
						}
					}
				}
			} catch (SocketTimeoutException ste) {
				Thread.currentThread().yield();
			} catch (IOException ioe) {
				ioe.printStackTrace();
			}
		}
		
		postRun();
	}
	
	public synchronized void messageReceived(Object message, int numLeftInQueue) throws Exception {
		// TODO: Batch messages (in a queue) and potentially write more than one message per DatagramPacket.
		if (message instanceof Message) {
			ByteBuffer fullOutBuffer = null;
			if (slaveGateway != null) {
				fullOutBuffer = slaveGateway.flattenMessage((Message)message);
				fullOutBuffer.order(ByteOrder.LITTLE_ENDIAN);
			} else {
				fullOutBuffer = ByteBuffer.allocate(((Message)message).flattenedSize());
				fullOutBuffer.order(ByteOrder.LITTLE_ENDIAN);
				((Message)message).flatten(fullOutBuffer);
				fullOutBuffer.rewind();
			}
			
			// Break the fullOutBuffer up into chunks of headers + data <= mtu.
			while (fullOutBuffer.remaining() > 0) {
				// Either the full remaining buffer + header, or MTU
				byte[] sendBuf = new byte[Math.min((fullOutBuffer.remaining() + FRAGMENT_HEADER_LENGTH), mtu)];
				ByteBuffer sendBuffer = ByteBuffer.wrap(sendBuf);
				sendBuffer.order(ByteOrder.LITTLE_ENDIAN);
				
				// Write the first four header fields
				sendBuffer.putInt(magic);
				sendBuffer.putInt(sexId);
				sendBuffer.putInt(sendMessageIDCounter);
				sendBuffer.putInt(fullOutBuffer.position()); // current offset of this message.
				
				// Process the chunk we need to send.
				byte[] chunkBuf = new byte[sendBuf.length - FRAGMENT_HEADER_LENGTH];
				fullOutBuffer.get(chunkBuf);
				
				// Write the next two headers.
				sendBuffer.putInt(chunkBuf.length);
				sendBuffer.putInt(fullOutBuffer.capacity());
				
				sendBuffer.put(chunkBuf);
				
				// Create the DatagramPacket, send the buffer.
				DatagramPacket packet = new DatagramPacket(sendBuf, sendBuf.length);
				packet = prepPacket(packet);
				socket.send(packet);
			}
		}
	}
	
	protected DatagramPacket prepPacket(DatagramPacket packet) {
		return packet;
	}
	
	
	// -- AbstractMessageIOGateway methods --
	private final class ReceiveState {
		int messageId;
		ByteBuffer buffer;
		
		ReceiveState() {
			this(0);
		}
		
		ReceiveState(int messageId) {
			messageId = messageId;
			buffer = null;
		}
	}
}
