#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

/** Port number used by my server */
#define PORT_NUMBER "54321"

// Type for an item name.
#define CLASS_ROOM_NAME_LENGTH 10
typedef char ClassRoom[ CLASS_ROOM_NAME_LENGTH + 1 ];

// List of all classrooms, as a resizable array.
static ClassRoom *classRoomList;

// List of all reserved rooms, as a resizable array.
static ClassRoom *reserveList;

// Total number of Class rooms available
static int maximumAvailableClassRoom = 5;



// Print out an error message and exit.
static void fail( char const *message ) {
  fprintf( stderr, "%s\n", message );
  exit( 1 );
}


//Initialize classrooms, Curreently they are set as ROOMXXXX
void initializeClassRooms(){
    
    for (int i = 0; i<maximumAvailableClassRoom; i++) {
        char room[11];
        sprintf(room, "ROOM%d", 1201+i);
        strcpy( classRoomList[i], room );
    }
    
}

/** handle a client connection, close it when we're done. */
void handleClient( int sock ) {
    
    FILE *fp = fdopen( sock, "a+" );
    
    // Prompt the user for a command.
    fprintf( fp, "> " );
    
    char cmd[ 11 ];
    int match;
    while ( ( match = fscanf( fp, "%s", cmd ) ) == 1 &&
           strcmp( cmd, "quit" ) != 0 ) {
        // Just echo the command back to the client.
    fprintf( fp, "%s\n", cmd );

    // Prompt the user for the next command.
    fprintf( fp, "> " );
    fflush( fp );
    }
    
    // Close the connection with this client.
    fclose( fp );
}

//initialize connection properties
int initializeSocket(){
    
    // Prepare a description of server address criteria.
    struct addrinfo addrCriteria;
    memset(&addrCriteria, 0, sizeof(addrCriteria));
    addrCriteria.ai_family = AF_INET;
    addrCriteria.ai_flags = AI_PASSIVE;
    addrCriteria.ai_socktype = SOCK_STREAM;
    addrCriteria.ai_protocol = IPPROTO_TCP;
    
    // Lookup a list of matching addresses
    struct addrinfo *servAddr;
    if ( getaddrinfo( NULL, PORT_NUMBER, &addrCriteria, &servAddr) )
        fail( "Can't get address info" );
    
    // Try to just use the first one.
    if ( servAddr == NULL )
        fail( "Can't get address" );
    
    // Create a TCP socket
    int servSock = socket( servAddr->ai_family, servAddr->ai_socktype,
                          servAddr->ai_protocol);
    if ( servSock < 0 )
        fail( "Can't create socket" );
    
    // Bind to the local address
    if ( bind(servSock, servAddr->ai_addr, servAddr->ai_addrlen) != 0 )
        fail( "Can't bind socket" );
    
    // Tell the socket to listen for incoming connections.
    if ( listen( servSock, 5 ) != 0 )
        fail( "Can't listen on socket" );
    
    // Free address list allocated by getaddrinfo()
    freeaddrinfo(servAddr);
    
    return servSock;
    

}

//initialize rooms, lock lists
//wait for incoming socket connections
int main() {
    
  classRoomList = (ClassRoom *) malloc( maximumAvailableClassRoom * sizeof( ClassRoom ) );
  reserveList = (ClassRoom *) malloc( maximumAvailableClassRoom * sizeof( ClassRoom ) );
  initializeClassRooms();
  //printAvailableClasses();
  int servSock = initializeSocket();
    
  // Fields for accepting a client connection.
  struct sockaddr_storage clntAddr; // Client address
  socklen_t clntAddrLen = sizeof(clntAddr);
    
  while ( true  ) {
    // Accept a client connection.
    int sock = accept( servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
    handleClient(sock);
  }
  

  // Stop accepting client connections (never reached).
  close( servSock );
  
  return 0;
}
