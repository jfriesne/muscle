package com.meyer.micromuscle.support;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

import javax.microedition.rms.RecordStore;
import javax.microedition.rms.RecordStoreException;

import com.meyer.micromuscle.message.Message;

/**
 * Encapsulates a Message for easier use as a Preference holder.
 * Stores flattened messages into a single Record within a J2ME RecordStore.
 *
 * @version 1.1 10.16.2007
 * @author Bryan Varner
 */
public class Preferences {
   RecordStore prefsRecords;
   protected Message prefs;
   
   /**
    * Creates a Preferences object with a blank message, that dosen't have a save file.
    */
   public Preferences() {
      prefs = new Message();
      prefsRecords = null;
   }
   
   /**
    * Creates a Preferences object with a default message, that dosen't have a save file.
    */
   public Preferences(Message defaults) {
      prefs = defaults;
      prefsRecords = null;
   }
   
   /**
    * Creates a new Preferences from the given file.
    * @param loadStore the File to load the preferences from.
    */
   public Preferences(RecordStore recordStore) {
      prefs = new Message();
      prefsRecords = recordStore;
	 
	 byte[] b = new byte[0];
	 try {
		 b = prefsRecords.getRecord(1);
	 } catch (Exception ex) { }
	 
      if (b.length > 0) {
		 ByteArrayInputStream bais = new ByteArrayInputStream(b);
		 try {
		    prefs.unflatten(new DataInputStream(bais), -1);
		 } catch (Exception e) {
		    // You are screwed.
		 }
	 }
   }
   
   /**
    * Creates a new Preferences instance using defaults for the initial settings,
    * then over-writing any defaults with the values from loadFile.
    * @param loadFile A Flattened Message to load as Preferences.
    * @param defaults An existing Message to use as the base.
    */
   public Preferences(RecordStore recordStore, Message defaults) {
      prefs = defaults;
      try {
         prefsRecords = recordStore;
	    ByteArrayInputStream bais = new ByteArrayInputStream(prefsRecords.getRecord(1));
	    
	    prefs.unflatten(new DataInputStream(bais), -1);
      } catch (Exception e) {
         // You are screwed.
      }
   }
   
   /**
    * Saves the preferences to the file they were opened from.
    * @throws RecordStoreException
    */
   public void save() throws RecordStoreException {
      if (prefsRecords != null) {
         save(prefsRecords);
      }
   }
   
   /**
    * Saves the preferences to the file specified.
    * @param saveAs the file to save the preferences to.
    * @throws RecordStoreException
    */
   public void save(RecordStore saveAs) throws RecordStoreException {
	 ByteArrayOutputStream baos = new ByteArrayOutputStream();
      try {
		 prefs.flatten(new DataOutputStream(baos));
	 } catch (java.io.IOException ex) {
		 throw new RecordStoreException("Failed to flatten message.\n");
	 }
	 
	 if (saveAs.getRecord(1) != null) {
		 saveAs.addRecord(baos.toByteArray(), 0, baos.size());
	 } else {
		 saveAs.setRecord(1, baos.toByteArray(), 0, baos.size());
	 }
   }
   
   /**
    * @return the Message this object is manipulating.
    */
   public Message getMessage() {
      return prefs;
   }
}
