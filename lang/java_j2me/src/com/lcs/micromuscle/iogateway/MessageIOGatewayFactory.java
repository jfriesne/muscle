package com.meyer.micromuscle.iogateway;

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
      if (encoding == AbstractMessageIOGateway.MUSCLE_MESSAGE_DEFAULT_ENCODING) return new MessageIOGateway();
	 System.out.println("New JZLibGateway...");
	 return new JZLibMessageIOGateway(encoding);
   }
}
