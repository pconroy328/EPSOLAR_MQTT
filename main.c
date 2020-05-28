/* 
 * File:   main.c
 * Author: pconroy
 *
 *  This program reads the data from an EPEver Solar Charge Controller and
 *  pumps it our as a JSON formatted message to an MQTT broker
 * 
 * Created on February 18, 2020, 4:12 PM
 * 
 * Libraries:
 *  -llibmqttrv -lepsolar -llog4c -lcjson -lmosquitto -lmodbus -lavahi-client -lavahi-common
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include "log4c.h"
#include "libmqttrv.h"
#include "libepsolar.h"




static  char    *version = "EPSOLAR_MQTT SCC Data Publisher - version 0.2 (direct connect to broker]";


static  int     sleepSeconds = 60;                  // How often to send out SCC data packets
static  char    *brokerHost = "mqttrv.local";       // default address of our MQTT broker
static  int     passedInBrokerHost = FALSE;         // TRUE if they passed it in via the command line
static  int     loggingLevel = 3;

static  char    *topTopic = "SCC";                  // MQTT top level topic
static  char    publishTopic[ 1024 ];               // published data will be on "<topTopic>/<controlleID>/DATA"
static  char    subscriptionTopic[ 1024 ];          // subscribe to <"<topTopic>/<controlleID>/COMMAND"

static  int     synchClocks  = TRUE;
static  int     controllerID = 1;
static  char    *devicePortName = NULL;


//  
// Forwards
static  void    parseCommandLine( int, char ** );
extern  char    *realTimeDataToJSON( const char *publishTopic, epsolarRealTimeData_t *rtData );
extern  void    *processInboundCommand( void * );



// -----------------------------------------------------------------------------
int main (int argc, char* argv[]) 
{    
    printf( "%s\n", version );
    
    parseCommandLine( argc, argv );
    Logger_Initialize( "/tmp/epsolar_mqtt.log", loggingLevel );           
    Logger_LogWarning( "%s\n", version );

    
    //
    //  Connect to the EPSolar Solar Charge Controller
    //  Pass in NULL to use default port defined in libepsolar/epsolar.c
    //  Second parmeter is Modbus Slave ID.  '1' is fine
    if (!epsolarModbusConnect( NULL, 1 )) {
        Logger_LogFatal( "Unable to open default device port %s to connect to the solar charge controller\n", epsolarGetDefaultPortName() );
        return( EXIT_FAILURE );
    }
    
    //
    // Create a FIFO queue for our incoming Commands over MQTT
    //createQueue( 0, 0 );

    //
    struct mosquitto *aMosquittoInstance;
    if (!passedInBrokerHost) {
        Logger_LogWarning( "No MQTT Broker host passed in on command line - trying mDNS to locate broker.\n" );
        MQTT_ConnectRV( &aMosquittoInstance, 60 );
    } else {
        Logger_LogWarning( "MQTT Broker host (%s) passed in on command line. Looking for JUST THAT ONE\n.", brokerHost );
        if (!MQTT_Initialize( brokerHost, 1883, &aMosquittoInstance )) {
            Logger_LogFatal( "Unable to find a broker by that name [%s] - we will exit.\n", brokerHost );
        }
    }
    
#if 0    
    //
    //  Start up a new thread to watch the Command Queue
    pthread_t   commandProcessingThread;
    if(pthread_create( &commandProcessingThread, NULL, processInboundCommand, NULL ) ) {
        Logger_LogFatal( "Unable to start the command processing thread!\n" );
        perror( "Error:" );
        return -1;
    }
#endif 
    

    //
    //  Concatenate topTopic and controller ID to create our Pub and Sub Topics
    snprintf( publishTopic, sizeof publishTopic, "%s/%d/%s", topTopic, controllerID, "DATA" );
    Logger_LogWarning( "Publishing messages to MQTT Topic [%s]\n", publishTopic );
    
    snprintf( subscriptionTopic, sizeof subscriptionTopic, "%s/%d/%s", topTopic, controllerID, "COMMAND" );
    Logger_LogWarning( "Subscribing to commands on MQTT Topic [%s]\n", subscriptionTopic );
    MQTT_Subscribe( aMosquittoInstance, subscriptionTopic, 0 );

        
    //
    //  Loop forever - read SCC data and send it out
    epsolarRealTimeData_t   realTimeData;
    
    while (TRUE) {
        if (synchClocks)
            eps_setRealtimeClockToNow();
        
        //
        //  every time thru the loop - zero out the structs!
        memset( &realTimeData, '\0', sizeof( epsolarRealTimeData_t ) );
        epsolarGetRealTimeData( &realTimeData );
        
        //
        // craft a JSON message from the data 
        char    *jsonMessage = realTimeDataToJSON( publishTopic, &realTimeData );
       
        //
        // Publish it to our MQTT broker; QoS = 0
        MQTT_Publish( aMosquittoInstance, publishTopic, jsonMessage, 0 );
        
        free( jsonMessage );
        sleep( sleepSeconds );
    }

    
    //
    // we never get here!
    MQTT_Unsubscribe( aMosquittoInstance, subscriptionTopic );
    MQTT_Teardown( aMosquittoInstance, NULL );

    //if (pthread_join( commandProcessingThread, NULL )) {
    //    Logger_LogError( "Shutting down but unable to join the commandProcessingThread\n" );
    //}    
    
    //destroyQueue();
    
    Logger_LogWarning( "Exiting (should not happen)" );
    Logger_Terminate();
    
    return( EXIT_SUCCESS );
}

// -----------------------------------------------------------------------------
static
void    showHelp()
{
    puts( "Options" );
    puts( "  -h  <string>   MQTT host to connect to" );
    puts( "  -t  <string>   MQTT top level topic" );
    puts( "  -s  N          sleep between sends <seconds>" );
    puts( "  -i  N          give this controller an identifier (defaults to 1)" );
    puts( "  -p  <string>   open this /dev/port to talk to contoller (defaults to /dev/ttyUSB0)" );
    puts( "  -v  N          logging level 1..5" );
    puts( "  -c             do NOT synch clocks (default is to synch)" );
    exit( 1 ); 
}

// -----------------------------------------------------------------------------
static
void    parseCommandLine (int argc, char *argv[])
{
    //
    //  Options
    //  -h  <string>    MQTT host to connect to
    //  -t  <string>    MQTT top level topic
    //  -s  N           sleep between sends <seconds>
    //  -i  <string>    give this controller an identifier (defaults to LS1024B_1)
    //  -p  <string>    open this /dev/port to talk to contoller (defaults to /dev/ttyUSB0
    char    c;
    
    while (((c = getopt( argc, argv, "h:t:s:i:p:v:j" )) != -1) && (c != 255)) {
        switch (c) {
            case 'h':   brokerHost = optarg;
                        passedInBrokerHost = TRUE;
                        break;
                        
            case 's':   sleepSeconds = atoi( optarg );  break;
            case 't':   topTopic = optarg;              break;
            case 'i':   controllerID = atoi( optarg );  break;
            case 'p':   devicePortName = optarg;        break;
            case 'v':   loggingLevel = atoi( optarg );  break;
            case 'c':   synchClocks = FALSE;            break;
            
            default:    showHelp();     break;
        }
    }
}

