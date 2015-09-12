/** \file

Simple minded example of reading/storing .cci file
*/

#include <cstdio>
#include <cstdlib>

#include "CML.h"
#include "can/can_copley.h"

// If a namespace has been defined in CML_Settings.h, this
// macros starts using it. 
CML_NAMESPACE_USE();

/* local functions */
static void showerr( const Error *err, const char *str );

/* local data */
int32 canBPS = 1000000;             // CAN network bit rate
const char *canDevice = "CAN0";     // Identifies the CAN device, if necessary
int16 canNodeID = 1;                // CANopen node ID

CopleyIOCfg cfg;


/**************************************************
* Read I/O from .cci file.
* Store to amp.
**************************************************/
int main( void )
{

   // The libraries define one global object of type
   // CopleyMotionLibraries named cml.
   //
   // This object has a couple handy member functions
   // including this one which enables the generation of
   // a log file for debugging
   cml.SetDebugLevel( LOG_EVERYTHING );

   // Create an object used to access the low level CAN network.
   // This examples assumes that we're using the Copley PCI CAN card.
   CopleyCAN can( canDevice );
   can.SetBaud( canBPS );

   // Open the CANopen network object
   CanOpen canOpen;
   const Error *err = canOpen.Open( can );
   showerr( err, "Opening CANopen network" );

   // Initialize the amplifier using default settings
   CopleyIO io;

   int line;

   err = io.Init( canOpen, canNodeID );
   showerr( err, "Initializing I/O module\n" );
   
   //insert load from file here
   err = io.LoadFromFile( "IOFileExample.cci", line );
   showerr( err, "Loading from file\n" );

   err = io.SaveIOConfig( );
   showerr( err, "Saving I/O config\n" );

   return 0;
}

/**************************************************/

static void showerr( const Error *err, const char *str )
{
   if( err )
   {
      printf( "Error %s: %s\n", str, err->toString() );
      exit(1);
   }
}

