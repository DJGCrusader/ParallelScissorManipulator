/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/**
\file
This file holds the code needed to implement CANopen Process Data Objects (PDOs).
*/

#include "CML.h"

CML_NAMESPACE_USE();

/**************************************************
* PDO Error objects
**************************************************/
CML_NEW_ERROR( PDO_Error, MapFull,       "The PDO Map is already full" );
CML_NEW_ERROR( PDO_Error, BitOverflow,   "Variable too long to add to map" );
CML_NEW_ERROR( PDO_Error, BitSizeError,  "The specified variable bit size is not supported" );

/***************************************************************************/
/**
Add the passed variable to the end of the variable map associated with this
PDO.
@param var The variable to be added.
@return An error object.
*/
/***************************************************************************/
const Error *PDO::AddVar( Pmap &var )
{
   // Make sure I'm not already full
   if( mapCt == PDO_MAP_LEN )
      return &PDO_Error::MapFull;

   byte bits = var.GetBits();

   // Keep the variable size <= 64 bits
   // NOTE - we no longer make this check.  EtherCAT and CAN-FD
   // both allow the possibility of PDOs longer then 8 bytes.
//   if( bitCt + bits > 64 )
//      return &PDO_Error::BitOverflow;

   // For now, my variables must be a multiple
   // of 8 bits long.
   if( bits & 0x07 )
      return &PDO_Error::BitSizeError;

   // Looks good, map it.
   map[mapCt++] = &var;
   bitCt += bits;
   return 0;
}

/***************************************************************************/
/**
Process data received from the network.  This function is called by the network
object when new data is received for this PDO.  It updates the values of the 
variables mapped to the PDO and calls the virtual Received() function.  
@param data Pointer to the newly received PDO data.
@param ct Size of the PDO data in bytes.
@param time System time stamp indicating the time of reception
*/
/***************************************************************************/
void TPDO::ProcessData( uint8 *data, int ct, uint32 time )
{
   timestamp = time;
   for( int i=0; i<mapCt; i++ )
   {
      int len = (map[i]->GetBits()>>3);
      if( len > ct ) break;
      map[i]->Set( data );
      data += len;
      ct -= len;
   }

   Received();
}

/***************************************************************************/
/**
Load the data from this PDO into the passed buffer.
@param buff Buffer where data will be loaded
@param max  The maximum number of bytes to load.
@return The actual number of bytes loaded.
*/
/***************************************************************************/
int RPDO::LoadData( uint8 *buff, int max )
{
   int tot = 0;

   // Load the data from the mapping objects
   for( int i=0; i<mapCt; i++ )
   {
      int ct = (map[i]->GetBits()>>3);
      if( ct > max ) break;

      map[i]->Get( &buff[tot] );
      tot += ct;
      max -= ct;
   }

   return tot;
}

