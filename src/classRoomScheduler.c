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

// List of all rooms we've reserved, as a resizable array.
static ClassRoom *reserveList;

// Number of rooms reserved.
static int reserveCount = 0;

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


//Initialize classrooms, Curreently they are set as ROOMXXXX
void initializeClassRooms(){
    
    for (int i = 0; i<maximumAvailableClassRoom; i++) {
        char room[11];
        sprintf(room, "ROOM%d", 1201+i);
        strcpy( classRoomList[i], room );
    }
    
}

//Reserve a classroom so another instance can't reserve it right away
void reserveClassRoom( char *classRoom,FILE *fp ) {
    pthread_mutex_lock( &monitorLock );
    
    // make sure the item we need is available.
    bool available = false;
    while ( !available ) {
        available = true;
        for ( int i = 0; i < reserveCount; i++ )
            if ( strcmp( classRoom, reserveList[ i ] ) == 0 )
                available = false;
        
        if ( !available ){
            pthread_cond_wait( &unlockCond, &monitorLock );
        }
    }
    
    strcpy( reserveList[ reserveCount++ ], classRoom );
    
    pthread_mutex_unlock( &monitorLock );
}


//Release reservation, allow other instances to reserve it
void freeClassRoom( char *classRoom ,FILE *fp) {
    pthread_mutex_lock( &monitorLock );
    
    // Find the given name on the lock list.
    for ( int i = 0; i < reserveCount; i++ ){
        if ( strcmp( classRoom, reserveList[ i ] ) == 0 ) {
            // Unlock it.
            reserveCount--;
            if(i!=reserveCount){
                strcpy( reserveList[ i ], reserveList[ reserveCount ] );
            }
            strcpy( reserveList[ reserveCount ], "" );
            pthread_cond_broadcast( &unlockCond );

        }
    }
    
    pthread_mutex_unlock( &monitorLock );

}


//Print out the classrooms that are not reserved
void printAvailableClassRooms(FILE *fp){
    
    pthread_mutex_lock( &monitorLock );
    
    fprintf(fp,"Class Rooms that are Available:\n");
    for ( int i = 0; i < maximumAvailableClassRoom; i++ ){
        int flag =1;
        for (int j = 0; j < reserveCount; j++)
        {

            if(strcmp(classRoomList[i], reserveList[j]) == 0){
            flag =0; 
            break;
           }
        }
        if(flag==1)fprintf(fp,"%s\n",classRoomList[ i ] );
    }
        
    
    
    pthread_mutex_unlock( &monitorLock );
    
}


void listLocks( FILE *fp ) {
    pthread_mutex_lock( &monitorLock );
    
    for ( int i = 0; i < reserveCount; i++ )
        fprintf( fp, "%s\n", reserveList[ i ] );
    
    pthread_mutex_unlock( &monitorLock );
}


//check if the given name matches a class room
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

        if(strcmp( cmd, "available" ) == 0 ){
            printAvailableClassRooms(fp);
        }
        else if(strcmp( cmd, "reserved" ) == 0 ){
            listLocks(fp);
        }
        else if(strcmp( cmd, "book" ) == 0 ){
            ClassRoom classRoom;
            fscanf(fp, "%10s", classRoom);
            if(classRoomExists(classRoom)){
                //fprintf(fp,"Class Room Available\n");
                reserveClassRoom( classRoom ,fp);
                fprintf(fp,"Class Room Booked!\n");

            }
            else fprintf(fp,"Class Room Doesn't Exist\n");
        }
        else if(strcmp( cmd, "free" ) == 0 ){
            ClassRoom classRoom;
            fscanf(fp, "%10s", classRoom);
            if(classRoomExists(classRoom)){
                freeClassRoom( classRoom ,fp);
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
