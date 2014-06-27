package com.meyer.muscle.iogateway;

public class MessageIOGatewayFactory 
{
   // This class is a factory and shouldn't be created anywhere
   private MessageIOGatewayFactory() 
   {
      // empty
   }
   
   public static AbstractMessageIOGateway getMessageIOGateway() 
   {
      return MessageIOGatewayFactory.getMessageIOGateway(AbstractMessageIOGateway.MUSCLE_MESSAGE_DEFAULT_ENCODING);
   }
   
   public static AbstractMessageIOGateway getMessageIOGateway(int encoding)
   {
      return getMessageIOGateway(encoding, Integer.MAX_VALUE);
   }
   
   public static AbstractMessageIOGateway getMessageIOGateway(int encoding, int maxIncomingMessageSize) 
   {
      MessageIOGateway ret = null;
      
      if (encoding == AbstractMessageIOGateway.MUSCLE_MESSAGE_DEFAULT_ENCODING) ret = new MessageIOGateway();
      else
      {
         // else, try to get the best zlib implementation currently available
         try {
            if (Class.forName("com.jcraft.jzlib.JZlib") != null)
            {
               // Found the JZlib library on the classpath, now return a reference to JZLibMessageIOGateway
               Class gatewayClass = Class.forName("com.meyer.muscle.iogateway.JZLibMessageIOGateway");
               ret = (MessageIOGateway) gatewayClass.getConstructor(new Class[] {Integer.TYPE}).newInstance(new Object[] {new Integer(encoding)});
            }
         } catch (Exception e) {
            // If any problems occur, we will fall back to the native class!
         } 
         if (ret == null) ret = new NativeZLibMessageIOGateway(encoding);
      }
      if (ret != null) ret.setMaximumIncomingMessageSize(maxIncomingMessageSize);
      return ret;
   }
}
