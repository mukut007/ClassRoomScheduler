import java.io.IOException;
import java.net.DatagramSocket;
import java.net.DatagramPacket;
import java.net.SocketTimeoutException;
import java.net.InetAddress;

/** Client program to request permissions information from the server, given
    a username, an object and an operation. */
public class PermissionsClient {
  /** Port number at which the client receives packets.  Has to be different
      from the server's port, in case we run on the same host. */
  public static final int CLIENT_PORT = 28124;

  public static void main( String[] args ) {
    // Make sure the command-line arguments are good.
    if ( args.length != 4 ) {
      System.err.println( "usage: PermissionsClient <server_host> <user> <object> <operation>" );
      System.exit( 1 );
    }
 
    // Packet for receiving messages, probably big enough for any message we get.
    byte[] recvBuffer = new byte [ 2048 ];
    DatagramPacket recvPacket = new DatagramPacket( recvBuffer, recvBuffer.length );
    
    try {
      // Make a socket for sending and receiving messages.
      DatagramSocket sock = new DatagramSocket( CLIENT_PORT );
      sock.setSoTimeout( 2000 );

      // Get the server's address.
      InetAddress[] addrList = InetAddress.getAllByName( args[ 0 ] );

      // Construct a packet containing the user's request.
      byte[] buffer = ( args[ 1 ] + " " + args[ 2 ] + " " + args[ 3 ] ).getBytes();
      DatagramPacket sendPacket = new DatagramPacket( buffer, buffer.length,
                                                      addrList[ 0 ],
                                                      PermissionsServer.SERVER_PORT );
      sock.send( sendPacket );
          
      // Wait to receive a response, but only wait so long.
      sock.receive( recvPacket );
          
      // Turn the response into a string, and print it.
      String response = new String( recvBuffer, 0, recvPacket.getLength() );
      System.out.print( response + "\n" );

    } catch ( SocketTimeoutException e ) {
      System.out.println( "No response" );
    } catch( IOException e ){
      System.err.println( "Error in communicating with the server" + e );
    } 
  }
}
