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

// List of all items we've locked, as a resizable array.
static ClassRoom *lockList;

// Number of items locked.
static int lockCount = 0;

// Total number of Class rooms available
static int maximumAvailableClassRoom = 5;

/** Lock for getting into the monitor. */
static pthread_mutex_t monitorLock = PTHREAD_MUTEX_INITIALIZER;

/** To make threads wait until an item is unlocked. */
static pthread_cond_t unlockCond = PTHREAD_COND_INITIALIZER;


// Print out an error message and exit.
static void fail( char const *message ) {
  fprintf( stderr, "%s\n", message );
  exit( 1 );
}

void initializeClassRooms(){
    
    for (int i = 0; i<maximumAvailableClassRoom; i++) {
        char room[11];
        sprintf(room, "ROOM%d", 1201+i);
        strcpy( classRoomList[i], room );
    }
    
}

void reserveClassRoom( char *classRoom ) {
    pthread_mutex_lock( &monitorLock );
    
    // make sure the item we need is available.
    bool available = false;
    while ( !available ) {
        available = true;
        for ( int i = 0; i < lockCount; i++ )
            if ( strcmp( classRoom, lockList[ i ] ) == 0 )
                available = false;
        
        if ( !available )
            pthread_cond_wait( &unlockCond, &monitorLock );
    }
    
    if ( lockCount >= maximumAvailableClassRoom ) {
        maximumAvailableClassRoom *= 2;
        lockList = realloc( lockList, maximumAvailableClassRoom * sizeof( classRoom ) );
    }
    strcpy( lockList[ lockCount++ ], classRoom );
    
    pthread_mutex_unlock( &monitorLock );
}

bool vacantClassRoom( char *classRoom ) {
    pthread_mutex_lock( &monitorLock );
    
    // Find the given name on the lock list.
    for ( int i = 0; i < lockCount; i++ )
        if ( strcmp( classRoom, lockList[ i ] ) == 0 ) {
            // Unlock it.
            strcpy( lockList[ i ], lockList[ --lockCount ] );
            pthread_cond_broadcast( &unlockCond );
            pthread_mutex_unlock( &monitorLock );
            return true;
        }
    
    pthread_mutex_unlock( &monitorLock );
    return false;
}

void listLocks( FILE *fp ) {
    pthread_mutex_lock( &monitorLock );
    
    for ( int i = 0; i < lockCount; i++ )
        fprintf( fp, "%s\n", lockList[ i ] );
    
    pthread_mutex_unlock( &monitorLock );
}

void printAvailableClassRooms(FILE *fp){
    
    pthread_mutex_lock( &monitorLock );
    
    fprintf(fp,"Class Rooms that are Available:\n");
    for ( int i = 0; i < maximumAvailableClassRoom; i++ )
        fprintf(fp,"%s\n",classRoomList[ i ] );
    
    pthread_mutex_unlock( &monitorLock );
    
}

bool classRoomExists(ClassRoom classRoom){
    
    for ( int i = 0; i < maximumAvailableClassRoom; i++){
        if(strcmp(classRoomList[i],classRoom)==0) return true;
    }
    return false;
}

/** handle a client connection, close it when we're done. */
void *handleClient( void *arg ) {
    
    int sock = *(int *)arg;
    free( arg );
    
    FILE *fp = fdopen( sock, "a+" );
    
    // Prompt the user for a command.
    fprintf( fp, "> " );
    
    char cmd[ 11 ];
    int match;
    while ( ( match = fscanf( fp, "%s", cmd ) ) == 1 &&
           strcmp( cmd, "quit" ) != 0 ) {
        // Just echo the command back to the client.
        if(strcmp( cmd, "available" ) == 0 ){
            printAvailableClassRooms(fp);
        }
        else if(strcmp( cmd, "book" ) == 0 ){
            ClassRoom classRoom;
            fscanf(fp, "%10s", classRoom);
            if(classRoomExists(classRoom)){
                //fprintf(fp,"Class Room Available\n");
                reserveClassRoom( classRoom );
                fprintf(fp,"Class Room Booked!\n");

            }
            else fprintf(fp,"Class Room Doesn't Exist\n");
        }
        else if(strcmp( cmd, "vacant" ) == 0 ){
            ClassRoom classRoom;
            fscanf(fp, "%10s", classRoom);
            if(classRoomExists(classRoom)){
                fprintf(fp,"Class Room Available\n");
                vacantClassRoom( classRoom );
            }
            else fprintf(fp,"Class Room Doesn't Exist\n");
        }
        else{
            fprintf( fp, "Unknown Command: %s\n", cmd );
            fflush( fp );
        }
        
        // Prompt the user for the next command.
        fprintf( fp, "> " );
        fflush( fp );
    }
    
    // Close the connection with this client.
    fclose( fp );
    return NULL;
}


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

int main() {
    
  classRoomList = (ClassRoom *) malloc( maximumAvailableClassRoom * sizeof( ClassRoom ) );
  lockList = (ClassRoom *) malloc( maximumAvailableClassRoom * sizeof( ClassRoom ) );
  initializeClassRooms();
  //printAvailableClasses();
  int servSock = initializeSocket();
    
  // Fields for accepting a client connection.
  struct sockaddr_storage clntAddr; // Client address
  socklen_t clntAddrLen = sizeof(clntAddr);
    
  while ( true  ) {
    // Accept a client connection.
    int sock = accept( servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);

    // Talk to this client
      int *sockPtr = (int *)malloc( sizeof( int ) );
      *sockPtr = sock;
      
      // Create a thread to handle communication witht his client.
      pthread_t thread;
      if ( pthread_create( &thread, NULL, handleClient, sockPtr ) != 0 )
          fail( "Can't create a client thread." );
      
      // Detach the thread, since we never need to join with it.
      pthread_detach( thread );
  }
  

  // Stop accepting client connections (never reached).
  close( servSock );
  
  return 0;
}
