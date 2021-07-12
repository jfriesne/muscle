package com.jcraft.jzlib.j2me;

import java.io.*;

/**
 * FilterInputStream re-implementation.
 * Allows the building and use of jzlib on J2ME.
 */
public class FilterInputStream extends InputStream {
	protected InputStream in;
	
	protected FilterInputStream(InputStream in) {
		this.in = in;
	}
	
	public int read() throws IOException {
		return in.read();
	}
	
	public int read(byte[] b) throws IOException {
		return in.read(b);
	}
	
	public int read(byte[] b, int off, int len) throws IOException {
		return in.read(b, off, len);
	}
	
	public long skip(long n) throws IOException {
		return in.skip(n);
	}
	
	public int available() throws IOException {
		return in.available();
	}
	
	public void close() throws IOException {
		in.close();
	}
	
	public void mark(int readlimit) {
		in.mark(readlimit);
	}
	
	public void reset() throws IOException {
		in.reset();
	}
	
	public boolean markSupported() {
		return in.markSupported();
	}
}