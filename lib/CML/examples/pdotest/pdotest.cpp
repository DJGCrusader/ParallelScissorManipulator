/*

This is a simple example showing how a transmit / receive PDO can be mapped
to a CANopen node using CML.

*/

#include <stdio.h>
#include <stdlib.h>

#include "CML.h"
#include "can/can_copley.h"

// If a namespace has been defined in CML_Settings.h, this
// macros starts using it. 
CML_NAMESPACE_USE();

/* local functions */
static void showerr( const Error *err, const char *str );

// Define a class based on the TPDO base class.
// A Transmit PDO is one that's transmitted by the
// CANopen node and received by the master.
// This PDO will be used to send the motor position
// and the analog reference value from the drive to the master.
class TPDO_Pos: public TPDO
{
   // These variables are used to map objects to the PDO.
   // Different mapping objects are used for different data sizes.
   // Position is a 32-bit value, analog input reading is 16-bit
   // A maximum of 8-bytes can be mapped to any PDO.
   Pmap32 pos;
   Pmap16 adc;
public:

   // Default constructor does nothing
   TPDO_Pos(){}

   // Called once at startup to map the PDO and configure CML to 
   // listen for it.
   const Error *Init( Node &node, int slot, int canID );

   // This function is called when the PDO is received
   virtual void Received( void );
};

// To send data to the drive we use a receive PDO.
// To create a custom one, derive a class from the RPDO base
class RPDO_off: public RPDO
{
   // Objects that keep track of what's mapped to the PDO
   // Again, up to 8 bytes total per PDO.  Here we're just
   // mapping the offset for the analog reference.
   Pmap16 off;

   // I keep a reference to the network to send on.
   uint32 netRef;

public:
   // Default constructor
   RPDO_off(){}

   // One time init
   const Error *Init( Node &node, int slot, int canID );

   // Call this function when we want to send the PDO data.
   const Error *SendData( int off );
};

/**************************************************
* Just home the motor and do a bunch of random
* moves.
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
   cml.SetFlushLog( true );

   // Create an object used to access the low level CAN network.
   // This examples assumes that we're using the Copley PCI CAN card.
   CopleyCAN can;
   can.SetBaud( 1000000 );

   // Open the CANopen network object
   CanOpen canOpen;
   const Error *err = canOpen.Open( can );
   showerr( err, "Opening CANopen network" );

   // Create an object to represent a node on the CANopen network
   Amp amp;
   amp.Init( canOpen, 1 );

   // Configure the transmit PDO
   // The second parameter gives the PDO 'slot' to use.  The amps have 8 transmit and 8 receive PDOs
   // so this number should range from 0 to 7.  CML itself uses TPDO slots 0, 1 and 4, so those shouldn't 
   // be used for custom PDOs
   //
   // The thrid parameter is a CAN message ID to use.  This needs to be unique in the system.  It's simplest
   // to use an extended message ID (i.e. one with bit 29 set) since this won't clash with any standard CANopen
   // ID used in the system.
   TPDO_Pos tpdo;
   err = tpdo.Init( amp, 2, 0x20000001 );
   showerr( err, "Initting TPDO" );

   // Configure the receive PDO.
   // CML uses RPDO slots 0 1 and 4, so I should use slot number 2,3,5-7.
   // Again CAN ID should be unique.  Easiest is to use an extended ID (bit 29 set)
   RPDO_off rpdo;
   err = rpdo.Init( amp, 2, 0x20000101 );
   showerr( err, "Initting RPDO" );

   // Send a PDO to reset the A/D offset on the drive to zero
   int offset = 0;
   rpdo.SendData( offset );

   // That's all for this thread.  The CANopen receiver thread will call a member
   // function of the mapped TPDO object when the corresponding CAN message is received.
   printf( "Finished with init\nPress enter to update the A/D offset value\n" );

   // Every time I enter a character, I'll set the A/D offset to a new value
   // This should be reflected in the value I read back on my transmit PDO
   while( getchar() != 'q' )
   {
      offset += 100;
      err = rpdo.SendData( offset );
      showerr( err, "Setting offset\n" );
   }

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

/**
 * Transmit PDO init function.
 */
const Error *TPDO_Pos::Init( Node &node, int slot, int canID )
{
   // Initialize the transmit PDO base class.
   // This needs to know the CANopen network and CAN message ID 
   // associated with the PDO.
   const Error *err = TPDO::Init( canID );

   // Set transmit type to 1.  This causes the PDO to be sent every 
   // sync period.  Setting the type to 2 would cause it to be sent every
   // second period, etc.
   if( !err ) err = SetType( 1 );

   // Init the two mapping objects to correspond to the 
   // data that will be mapped to this PDO
   if( !err ) err = pos.Init( 0x6064, 0 );  // motor position
   if( !err ) err = adc.Init( 0x2200, 0 );  // Analog reference

   // Add the mapped variables to the PDO
   if( !err ) err = AddVar( pos );
   if( !err ) err = AddVar( adc );

   // Program this PDO in the node and enable it
   if( !err ) err = node.PdoSet( slot, *this );

   return err;
}

/**
 * This function will be called by the high priority CANopen receive
 * thread when the PDO is received.  
 *
 * By the time this function is called, the data from the two mapped objects will
 * have been parsed from the input message.  It can be accessed by the Pmap objects
 * that we created.
 *
 * Keep in mind that this function is called from the same thread that receives all
 * CANopen messages.  Keep any processing here short and don't try to do any SDO access.
 * Often it's best to simply post a semaphore here and have another thread handle the data.
 *
 */
void TPDO_Pos::Received( void )
{
   printf( "PDO received - position: %-9d adc: %-5d \r", pos.Read(), adc.Read() );
}


// One time init of my receive PDO object
const Error *RPDO_off::Init( Node &node, int slot, int canID )
{
   netRef = node.GetNetworkRef();

   // Init the base class
   const Error *err = RPDO::Init( canID );

   // Init the mapping variables that define the mapped objects
   if( !err ) err = off.Init( 0x2311 );

   // Add the variable to the PDO
   if( !err ) err = AddVar( off );

   // Set the PDO type so that it's data will be acted on immediately
   if( !err ) err = SetType( 255 );

   // Program this PDO
   if( !err ) err = node.PdoSet( slot, *this );

   return err;
}

// Call this function when we want to send the PDO data.
const Error *RPDO_off::SendData( int val )
{
   // Update the mapping object with the value to send.
   off.Write( val );

   // Lock the network reference.  This converts the 
   // reference into a pointer to the network in a safe way
   // The reference will be unlocked when this function returns
   RefObjLocker<Network> net( netRef );
   if( !net ) return &NodeError::NetworkUnavailable;

   // Now, send the PDO 
   return RPDO::Transmit( *net );
}

