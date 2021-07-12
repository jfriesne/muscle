namespace muscle.test {
  using System;
  using System.IO;
  using System.Net;
  using System.Net.Sockets;
  using System.Text;
  using System.Collections;
  using System.Diagnostics;
  using System.Threading;

  using muscle.message;
  using muscle.client;
  using muscle.iogateway;

  public class ExampleClient {
    static void Main() {
      string [] args = Environment.GetCommandLineArgs();

      string host = "127.0.0.1";
      int port = 2960;

      if (args.Length >= 2)
	host = args[1];

      if (args.Length >= 3)
	port = System.Int32.Parse(args[2]);

      new ExampleClient(host, port); 
    }
      
    public ExampleClient(string hostName, int port)
    {
      Client client = new Client(hostName, port);

      client.RegisterForMessages(new MessagesCallback(MessagesCallback), null);
      client.RegisterForDisconnect(new DisconnectCallback(DisconnectCallback),
				   null);

      client.BeginConnect(new AsyncCallback(this.ConnectCallback), client);

      Message m1 = new Message();

      m1.what = StorageReflectConstants.PR_COMMAND_SETPARAMETERS;
      m1.setBoolean("SUBSCRIBE:*", true);
      m1.setBoolean("SUBSCRIBE:*/*", true);
      m1.setBoolean("SUBSCRIBE:*/*/*", true);
      m1.setBoolean("SUBSCRIBE:*/*/*/*", true);
      m1.setBoolean("SUBSCRIBE:*/*/*/*/*", true);

      client.Send(m1);

      while (true) {
	Thread.Sleep(5000);
      }
    }

    public void ConnectCallback(IAsyncResult ar) 
    {
      Client client = (Client) ar.AsyncState;
      client.EndConnect(ar);
    }

    public void DisconnectCallback(Client client,
				   Exception err,
				   object state)
    {
      Console.WriteLine(err.ToString());
      Console.WriteLine("disconnected");

    }

    public void MessagesCallback(Message [] messages,
				 Client client, 
				 object state) 
    {
      foreach (Message message in messages) {
	System.Console.WriteLine(message.ToString());
      }
    }
    
  }
}
