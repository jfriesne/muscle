package com.meyer.muscle.support;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

import com.meyer.muscle.message.Message;

/**
 * Encapsulates a Message for easier use as a Preference holder.
 *
 * @version 1.0 1.22.2003
 * @author Bryan Varner
 */
public class Preferences {
   File prefsFile;
   protected Message prefs;
   
   /**
    * Creates a Preferences object with a blank message, that dosen't have a save file.
    */
   public Preferences() {
      prefs = new Message();
      prefsFile = null;
   }
   
   /**
    * Creates a Preferences object with a default message, that dosen't have a save file.
    */
   public Preferences(Message defaults) {
      prefs = defaults;
      prefsFile = null;
   }
   
   /**
    * Creates a new Preferences from the given file.
    * @param loadFile the File to load the preferences from.
    */
   public Preferences(String loadFile) throws IOException {
      prefs = new Message();
      prefsFile = new File(loadFile);
      DataInputStream fileStream = new DataInputStream(new FileInputStream(prefsFile));
      try {
         prefs.unflatten(fileStream, -1);
      } catch (Exception e) {
         // You are screwed.
      }
   }
   
   /**
    * Creates a new Preferences instance using defaults for the initial settings,
    * then over-writing any defaults with the values from loadFile.
    * @param loadFile A Flattened Message to load as Preferences.
    * @param defaults An existing Message to use as the base.
    */
   public Preferences(String loadFile, Message defaults) {
      prefs = defaults;
      try {
         prefsFile = new File(loadFile);
         DataInputStream fileStream = new DataInputStream(new FileInputStream(prefsFile));
         prefs.unflatten(fileStream, -1);
      } catch (Exception e) {
         // You are screwed.
      }
   }
   
   /**
    * Saves the preferences to the file they were opened from.
    * @throws IOException
    */
   public void save() throws IOException {
      if (prefsFile != null) {
         save(prefsFile.getAbsolutePath());
      }
   }
   
   /**
    * Saves the preferences to the file specified.
    * @param saveAs the file to save the preferences to.
    * @throws IOException
    */
   public void save(String saveAs) throws IOException {
      DataOutputStream fileStream = new DataOutputStream(new FileOutputStream(saveAs, false));
      prefs.flatten(fileStream);
      fileStream.close();
   }
   
   /**
    * @return the Message this object is manipulating.
    */
   public Message getMessage() {
      return prefs;
   }
}
