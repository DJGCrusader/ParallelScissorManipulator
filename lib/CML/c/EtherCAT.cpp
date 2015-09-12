/*******************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/*************************************************************************************************************/
/*                                                                                                           */
/* Copyright (C) 2010 [###]                                                                                  */
/*                                                                                                           */
/* The EtherCAT Technology, the trade name and logo “EtherCAT” are the intellectual property of,             */
/* and protected by Beckhoff Automation GmbH. You may use the EtherCAT Copley Control Library                */
/* for the sole purpose of creating and/or selling or otherwise distributing an EtherCAT network master      */
/* provided that an EtherCAT Master License is obtained from Beckhoff Automation GmbH.                       */
/*                                                                                                           */
/* The EtherCAT Master License is available free of charge from Beckhoff Automation GmbH,                    */
/* Eiserstraße 5, D-33415 Verl, Germany (www.beckhoff.com).                                                  */
/*                                                                                                           */
/*************************************************************************************************************/

/** \file

This file holds code for the top level EtherCAT class.
This class manages the EtherCAT network.
*/

#include <string.h>
#include "CML.h"

CML_NAMESPACE_USE();

// static EtherCAT error objects
CML_NEW_ERROR( EtherCatError, ThreadStart,     "Error starting EtherCAT monitoring thread"            );
CML_NEW_ERROR( EtherCatError, OpenHardware,    "Unable to open EtherCAT hardware"                     );
CML_NEW_ERROR( EtherCatError, ReadHardware,    "Error reading from Ethernet socket"                   );
CML_NEW_ERROR( EtherCatError, WriteHardware,   "Error writing to Ethernet socket"                     );
CML_NEW_ERROR( EtherCatError, NoResponse,      "Remote device did not respond to request (working counter is zero)." );
CML_NEW_ERROR( EtherCatError, EcatNotInit,     "EtherCAT network object not initialized"              );
CML_NEW_ERROR( EtherCatError, NodeNotFound,    "Specified node was not found on the EtherCAT network" );
CML_NEW_ERROR( EtherCatError, NodeBootMode,    "EtherCAT node is currently in boot mode."             );
CML_NEW_ERROR( EtherCatError, NodeStateChange, "Error changing node operational state."               );
CML_NEW_ERROR( EtherCatError, EcatMsgCorrupt,  "EtherCAT message received from network is corrupt."   );
CML_NEW_ERROR( EtherCatError, DatagramWontFit, "Not enough space in frame for new datagram"           );
CML_NEW_ERROR( EtherCatError, NodeNotInit,     "EtherCAT node has not been initialized"               );
CML_NEW_ERROR( EtherCatError, PdoNotEnabled,   "PDO is not currently enabled on network"              );
CML_NEW_ERROR( EtherCatError, Sync0Config,     "Error configuring SYNC0 timer on slave device"        );

CML_NEW_ERROR( EtherCatError, MboxError,       "EtherCAT node returned an unknown mailbox error code"               );
CML_NEW_ERROR( EtherCatError, MboxSyntax,      "EtherCAT node reported the syntax of mailbox header is invalid"     );
CML_NEW_ERROR( EtherCatError, MboxProtocol,    "EtherCAT node does not support the requested mailbox protocol"      );
CML_NEW_ERROR( EtherCatError, MboxChannel,     "EtherCAT node returned an invalide mailbox channel code"            );
CML_NEW_ERROR( EtherCatError, MboxService,     "EtherCAT node does not support the requested mailbox service"       );
CML_NEW_ERROR( EtherCatError, MboxHeader,      "EtherCAT node reported an invalid mailbox protocol header"          );
CML_NEW_ERROR( EtherCatError, MboxTooShort,    "EtherCAT node reported the length of the mailbox data is too short" );
CML_NEW_ERROR( EtherCatError, MboxMemory,      "EtherCAT node reported insufficient memory for mailbox transfer"    );
CML_NEW_ERROR( EtherCatError, MboxSize,        "EtherCAT node reported inconsistent mailbox data length"            );
CML_NEW_ERROR( EtherCatError, FoEformat,       "EtherCAT node returned incorrectly formatted FoE response"          );
CML_NEW_ERROR( EtherCatError, FoEerror,        "EtherCAT node returned an FoE error response."                      );
CML_NEW_ERROR( EtherCatError, NetworkWiringError, "EtherCAT network is not correctly wired"                         );

// Short delay to wait for amp to respond.
#ifdef CML_ALLOW_FLOATING_POINT
#  define SHORT_DELAY   0.25f
#else
#  define SHORT_DELAY   1
#endif

CML_NAMESPACE_START()

EcatDgram::EcatDgram( void ){ Init( 0, 0, 0, 0 ); }

/// Construct a datagram with a variable data length.
/// @param cmd Identifies the type of datagram
/// @param adp Generally the EtherCAT node on the network
/// @param ado Generally identifies the memory address being accessed.
/// @param len Length of the memory access in bytes
/// @param ptr Pointer where the read/write data is held
EcatDgram::EcatDgram( uint8 cmd, int16 adp, int16 ado, int16 len, void *ptr )
{
   Init( cmd, adp, ado, len, ptr );
}

/// Construct a datagram with up to 4 bytes of data
/// @param cmd Identifies the type of datagram
/// @param adp Generally the EtherCAT node on the network
/// @param ado Generally identifies the memory address being accessed.
/// @param len Length of the memory access in bytes (up to 4)
/// @param val Value of the data to send.
EcatDgram::EcatDgram( uint8 cmd, int16 adp, int16 ado, int16 len, int32 val )
{
   Init( cmd, adp, ado, len, val );
}

/// Default destructor for an EtherCAT datagram
EcatDgram::~EcatDgram(){}

/// Initialize a datagram with up to 4 bytes of data.  The data 
/// value will be stored in a local buffer
/// @param cmd Identifies the type of datagram
/// @param adp Generally the EtherCAT node on the network
/// @param ado Generally identifies the memory address being accessed.
/// @param len Length of the memory access in bytes (up to 4)
/// @param val Value of the data to send.
void EcatDgram::Init( uint8 cmd, int16 adp, int16 ado, int16 len, int32 val )
{
   CML_ASSERT( len <= 4 );
   int32_to_bytes( val,  local );
   Init( cmd, adp, ado, len, local );
}

/// Initialize a datagram with an arbitrary amount of data.
/// @param cmd Identifies the type of datagram
/// @param adp Generally the EtherCAT node on the network
/// @param ado Generally identifies the memory address being accessed.
/// @param len Length of the memory access in bytes
/// @param ptr Pointer where the read/write data is held
void EcatDgram::Init( uint8 cmd, int16 adp, int16 ado, int16 len, void *ptr )
{
   CML_ASSERT( len <= 0x7FF );
   this->cmd = cmd;
   this->adp = adp;
   this->ado = ado;
   this->len = len;
   this->dat = (uint8*)ptr;
   framePtr = 0;
   next = 0; 
   index = 0;
}

/// Reset the datagram
void EcatDgram::Reset( void )
{
   next = 0;
}

/// Add a new datagram after this one.
/// @param n Pointer to the next datagram.  A local copy of this pointer will be stored
void EcatDgram::setNext( EcatDgram *n )
{
   CML_ASSERT( framePtr );
   framePtr[7] |= 0x80;
   next = n;
}

/// Get a pointer to the next datagram stored in the frame.
/// @return A pointer to the next datagram, or NULL if there is none.
EcatDgram *EcatDgram::getNext( void )
{
   return next;
}

/// Pack this datagram into a memory buffer that's large enough to
/// hold an entire EtherCAT frame.
/// @param ptr   Points to the frame buffer
/// @param off   Gives the offset in the frame where this datagram should be loaded.
///              On return, this is increased by the size of the datagram
/// @return An error code or null on success.  
const Error *EcatDgram::Load( void *ptr, int16 &off )
{
   int tot = getDgramLen();
   if( off+tot > MAX_ECAT_FRAME )
      return &EtherCatError::DatagramWontFit;

   uint8 *buff = (uint8*)ptr;
   framePtr = &buff[off];

   memset( framePtr, 0, tot );
   framePtr[0] = cmd;
   framePtr[1] = index;
   int16_to_bytes( adp,  &framePtr[2] );
   int16_to_bytes( ado,  &framePtr[4] );
   int16_to_bytes( len,  &framePtr[6] );
   if( dat ) memcpy( &framePtr[10], dat, len );
   off += tot;
   return 0;
}

/// Compare the index stored in the frame that this datagram is part of
/// to the expected value.  This can be used to help verify that the 
/// frame received over the EtherCAT network contains the expected datagrams.
/// @return true if the index stored in the frame holds the expected value.
bool EcatDgram::checkNdx( void )
{
   if( framePtr )
      return framePtr[1] == index;
   else
      return false;
}

/// Update the data value stored in this datagram
/// @param ptr Pointer to the data to store.  The data referenced
///            by this pointer should be at least as long as the 
///            length of the datagram.
void EcatDgram::setData( void *ptr )
{
   if( dat )
      memcpy( dat, ptr, len );
   if( framePtr )
      memcpy( &framePtr[10], ptr, len );
}

/// Update the data value stored in this datagram
/// @param val The value to be stored in the datagram.
///            The datagram is expected to be no longer then 4 bytes.
void EcatDgram::setData( int32 val )
{
   int32_to_bytes( val, local );
   int l = (len<=4) ? len : 4;

   if( (dat != local) && dat )
      memcpy( dat, local, l );

   if( framePtr )
      memcpy( &framePtr[10], local, l );
}

/// Update the index value associated with this datagram
/// @param ndx The new index value to store.
void EcatDgram::setNdx( uint8 ndx )
{
   index = ndx;
   if( framePtr ) framePtr[1] = ndx;
}

/// Return the current datagram index value
/// @return The current index value.
uint8 EcatDgram::getNdx(void)
{
   if( framePtr ) return framePtr[1];
   return index;
}

/// Return the datagram length in bytes.  That's the length of the data plus 
/// the 12 byte header.
/// @return The datagram length in bytes.
int16 EcatDgram::getDgramLen( void ){ return len+12; }

int16 EcatDgram::getADP( void ){ CML_ASSERT( framePtr ); return bytes_to_int16( &framePtr[2] ); }
int16 EcatDgram::getADO( void ){ CML_ASSERT( framePtr ); return bytes_to_int16( &framePtr[4] ); }
int16 EcatDgram::getWKT( void ){ CML_ASSERT( framePtr ); return bytes_to_int16( &framePtr[len+10]); }
int16 EcatDgram::getData16s( void ){ CML_ASSERT( framePtr ); return bytes_to_int16( &framePtr[10] ); }
uint16 EcatDgram::getData16u( void ){ CML_ASSERT( framePtr ); return bytes_to_uint16( &framePtr[10] ); }
int32 EcatDgram::getData32s( void ){ CML_ASSERT( framePtr ); return bytes_to_int32( &framePtr[10] ); }
uint32 EcatDgram::getData32u( void ){ CML_ASSERT( framePtr ); return bytes_to_uint32( &framePtr[10] ); }
int16 EcatDgram::getData( void *ptr, int16 max )
{
   CML_ASSERT( framePtr ); 
   int16 l = (max<len) ? max : len;
   memcpy( ptr, &framePtr[10], l );
   return l;
}
void *EcatDgram::getDataPtr( void ){ CML_ASSERT( framePtr ); return &framePtr[10]; }
int16 EcatDgram::getDataLen( void ){ return len; }
const Error *EcatDgram::NewData( void ){ return 0;}

EcatFrame::EcatFrame( void ): refDgram( -1, 0, 4, 0 )
{
   SetRefName( "EcatFrame" );

   // The default MAC addresses are:
   //   dest: 255.255.255.255.255.255
   //   source: 0.1.2.3.4.5
   //
   // These choices are somewhat arbitrary
   for( int i = 0; i<6; i++ )
   {
      dstMAC[i] = 0xFF;
      srcMAC[i] = i;
   }

   Reset();
}

void EcatFrame::Reset( void )
{
   last = 0;

   // Clear the first 60 bytes of the frame.
   // The minimum frame size is 60 bytes, so these zeros
   // will be padding if not initialized later.
   memset( buff, 0, 60 );

   // The first 14 bytes give the destination & source MAC addresses
   // and the Ethertype code which identifies this as an EtherCAT 
   // message.
   memcpy( &buff[0], dstMAC, 6 );
   memcpy( &buff[6], srcMAC, 6 );
   buff[12] = 0x88;
   buff[13] = 0xA4;

   // The next two bytes (zeros for now) give the total size of the 
   // EtherCAT data contained in this frame.  I update this value each
   // time I add a datagram to the frame.

   // Size of the EtherCAT frame header is 16 bytes.  This value will be updated
   // as datagrams are added to the frame
   size = 16;

   // Add the first reference datagram.  I use this to keep track of the frame 
   refDgram.Reset();
   Add( &refDgram );
}

// Assign a 32-bit ID value to this frame and also an 8-bit
// index value.  These values are stored in the first dummy 
// datagram at the beginning of the frame.  They are used to
// identify the frame on it's return from the network
void EcatFrame::SetFrameID( uint8 index, uint32 id )
{
   refDgram.setNdx( index );
   refDgram.setData( id );
}

uint32 EcatFrame::GetFrameID( void )
{
   return refDgram.getData32u();
}

uint8 EcatFrame::GetFrameIndex( void )
{
   return refDgram.getNdx();
}

// Pull the frame ID value and index out of the passed buffer 
// assuming that this buffer contains a raw Ethernet frame 
// received over the wire.
//   @param len  Length of frame in bytes
//   @param buff Raw frame data
//   @param id   The 32-bit frame ID is returned here
//   @return the index value or 0 on failure
uint8 EcatFrame::FindFrameID( uint16 len, uint8 *buff, uint32 &id )
{
   // Check for minimum required length
   if( len < 32 ) return 0;

   // Check for proper EtherType value
   if( buff[12] != 0x88 ) return 0;
   if( buff[13] != 0xA4 ) return 0;

   // Make sure first datagram is APWR
   if( buff[16] != 2 ) return 0;

   // Get the ADP value.  This started at 1 and was incremented 
   // by each node as the frame passed around the network.  I'll 
   // fail if this is still set to 1.  This allows me to catch 
   // unprocessed packets which are returned by some Ethernet hardware
   // (i.e. winpcap).
   // The reason I don't use a NOP for my first frame is because this 
   // check wouldn't work with a NOP.
   if( bytes_to_int16( &buff[18] ) == 1 ) return 0;

   // Grab the reference value from this datagram and return it.
   id = bytes_to_uint32( &buff[26] );
   return buff[17];
}

EcatFrame::~EcatFrame()
{
   KillRef();
}

const Error *EcatFrame::WaitResponse( Timeout timeout )
{
   return sem.Get(timeout);
}

int16 EcatFrame::getSize( void )
{
   // Add padding if the size of the frame will be less
   // then 64 bytes (counting the 4 byte crc that I 
   // don't have to add here).
   if( size < 60 ) return 60;
   return size;
}

uint8 *EcatFrame::getBuff( void ){ return buff; }

// Add a datagram to this frame
const Error *EcatFrame::Add( EcatDgram *dg )
{
   dg->setNdx( dgIndex );

   const Error *err = dg->Load( buff, size );
   if( err ) return err;

   if( last )
      last->setNext( dg );
   last = dg;

   dgIndex++;
   int16_to_bytes( 0x1000 | (size-16), &buff[14] );
   return 0;
}

// Parse the response
const Error *EcatFrame::Process( uint8 *resp, int16 len )
{
   const Error *err;

   // Make sure this is an EtherCAT frame
   if( resp[12] != 0x88 ) return &EtherCatError::EcatMsgCorrupt;
   if( resp[13] != 0xA4 ) return &EtherCatError::EcatMsgCorrupt;

   // Make sure the size matches what I expect
   if( len != getSize() )
      return &EtherCatError::EcatMsgCorrupt;

   memcpy( buff, resp, len );

   for( EcatDgram *dg=&refDgram; dg; dg=dg->getNext() )
   {
      if( !dg->checkNdx() )
         return &EtherCatError::EcatMsgCorrupt;

      // Check if the response contains valid data
      err = dg->NewData();
      if( err ) return err;
   }

   sem.Put();
   return 0;
}

bool EcatFrame::IsEmpty( void )
{
   return refDgram.getNext() == 0;
}

struct PDO_Info
{
   uint32 ref;        // Reference to the PDO
   uint16 objID;      // CANopen object ID (0x1600, 0x1A02, etc)
   int16 byteLen;     // Length of PDO data in bytes
   int16 offset;      // Offset of PDO data in buffer
};

// This class keeps track of the enabled PDOs currently mapped.
// Each node has two lists, one for transmit PDOs (received from drive)
// and one for receive PDOs (sent to drive)
class PDO_List: public EcatDgram
{
protected:
   Mutex mtx;              // Mutex used to lock the list while updating it
   Array<PDO_Info> pdos;   // Dynamic array with some info for each mapped PDO
   uint8 *buff;            // Buffer which holds data passed to/from PDO
   uint16 syncObj;         // CANopen object ID.  This object has a list of enabled PDOs
   uint16 mapBase;         // CANopen object ID.  First PDO mapping object of this type
   uint16 byteCt;          // Total number of bytes of all PDOs in this list
   uint16 ramAddr;         // Address of sync manager RAM for this PDO data
   uint16 smBase;          // Register address of the sync manager used for this PDO type
   uint16 smCtrl;          // Value to write to sync manager control registers
   uint8 dgType;           // Type of datagram used to access this type of PDO data
   uint32 nodeRef;         // Reference to the node which owns this PDO list
   bool enabled;

   // Update the sync manager on the node which is used to read/write these PDOs.
   // This is called any time a PDO is added/removed from the list
   const Error *UpdtSyncMgr( Node *node )
   {
      const Error *err;
      byteCt = 0;

      // Grab a reference to the node
      nodeRef = node->GrabRef();

      // The PDO needs to be disabled before this is called.
      CML_ASSERT( !enabled );

      err = node->sdo.Dnld8( syncObj, 0, (byte)0 );
      if( err ) return err;

      int ct = pdos.length();
      for( int i=0; i<ct; i++ )
      {
         // Get a pointer to the PDO structure.
         PDO *pdo = (PDO*)RefObj::LockRef( pdos[i].ref );

         // If it's been deleted for some reason, just ignore it
         if( !pdo ) continue;

         // Round it's length up to the nearest byte, and add this to my total length
         pdos[i].offset = byteCt;
         pdos[i].byteLen = (pdo->GetBitCt()+7)/8;
         byteCt += pdos[i].byteLen;

         pdo->UnlockRef();

         err = node->sdo.Dnld16( syncObj, i+1, pdos[i].objID );
         if( err ) return err;
      }
   
      err = node->sdo.Dnld8( syncObj, 0, (byte)ct );
      if( err ) return err;

      // Make sure at least one byte was mapped.  This is important for the receive PDO sync manager
      // since it ensures that we write to the SM even if no PDOs are mapped.  That keeps the heartbeat
      // going.
      if( !byteCt ) byteCt = 1;

      // Allocate a buffer that will hold the raw data sent to/from this set of PDOs
      delete[] buff;
      buff = new uint8[ byteCt ];
      InitBuffer();

      // Initialize my datagram.  This datagram is sent by the cyclic thread to either 
      // read or write the process data for this node.
      RefObjLocker<EtherCAT> ecat( node->GetNetworkRef() );
      if( !ecat ) return &EtherCatError::EcatNotInit;

      uint16 nodeAddr;
      err = ecat->GetNodeAddress( node, nodeAddr );
      if( err ) return err;
      EcatDgram::Init( dgType, nodeAddr, ramAddr, byteCt, buff );

      // Update the sync manager length
      err = ecat->CfgSyncMgr( node, smBase, ramAddr, byteCt, smCtrl );
      if( err ) return err;

      // If everything went well, we can enable the PDO
      enabled = true;
      return 0;
   }

   virtual void InitBuffer( void )
   {
      memset( buff, 0, byteCt );
   }

   // This extends the virtual load function of the EcatDgram class.
   // Here we just lock the mutex before calling the parent's Load 
   // method.  This ensures that we wont load the frame while we're 
   // updating the contents of the local buffer
   virtual const Error *Load( uint8 *buff, int16 &off )
   {
      MutexLocker ml( mtx );
      return EcatDgram::Load(buff,off);
   }

   // Protected constructor.
   // This class is only intended to be used by it's sub-classes
   PDO_List( uint16 syncObj, uint16 mapBase, uint16 smBase, uint16 smCtrl, uint8 dgType ): pdos(32)
   {
      this->syncObj = syncObj;
      this->mapBase = mapBase;
      this->smBase  = smBase;
      this->smCtrl  = smCtrl;
      this->dgType  = dgType;

      buff    = 0;
      byteCt  = 0;
      ramAddr = 0;
      enabled = false;
   }

public:
   ~PDO_List()
   {
      for( int i=0; i<pdos.length(); i++ )
         RefObj::ReleaseRef( pdos[i].ref );
      delete[] buff;
   }

   bool isEnabled( void )
   {
      return enabled;
   }

   // Add the PDO to my list of enabled PDOs
   const Error *AddPDO( Node *node, PDO *pdo, int slot )
   {
      PDO_Info pi;
      pi.ref   = pdo->GrabRef();
      pi.objID = mapBase + slot;

      mtx.Lock();
      enabled = false;
      pdos.add(pi);
      mtx.Unlock();

      return UpdtSyncMgr( node );
   }

   // Remove a PDO from my list of enabled PDOs
   const Error *RemPDO( Node *node, int slot )
   {
      uint16 objID = mapBase + slot;

      mtx.Lock();
      int ct = pdos.length();
      for( int i=0; i<ct; i++ )
      {
         if( pdos[i].objID == objID )
         {
            RefObj::ReleaseRef( pdos[i].ref );
            pdos.rem(i);
            break;
         }
      }

      enabled = false;
      mtx.Unlock();

      return UpdtSyncMgr( node );
   }

   // Remove this PDO from any location in the list of enabled PDOs
   const Error *RemPDO( Node *node, PDO *pdo )
   {
      uint32 pdoRef = pdo->RefID();

      mtx.Lock();
      bool found;
      do
      {
         found = false;
         for( int i=0; i<pdos.length(); i++ )
         {
            if( pdos[i].ref == pdoRef )
            {
               RefObj::ReleaseRef( pdoRef );
               pdos.rem(i);
               found = true;
               break;
            }
         }
      } while( found );

      enabled = false;
      mtx.Unlock();

      return UpdtSyncMgr( node );
   }

   void SetSyncRamAddr( uint16 addr )
   {
      enabled = false;
      ramAddr = addr;
   }
};

class TPDO_List: public PDO_List
{
public:
   TPDO_List( void ): PDO_List( 0x1C13, 0x1A00, 0x0818, 0x0020, 4 ){};

   // This function is called by the receive thread when a response
   // to the PDO is received.
   const Error *NewData( void )
   {
      MutexLocker ml( mtx );

      // I ignore if no TPDOs are enabled
      if( !enabled )
         return 0;

      // Make sure the working counter isn't zero.
      // If it is, notify any threads of the change in node state
      if( !getWKT() )
      {
         RefObjLocker<Node> nodePtr( nodeRef );
         if( nodePtr )
            nodePtr->SetState( NODESTATE_GUARDERR );
      }

      // Find a pointer to the frame buffer holding the received data
      uint8 *datPtr = (uint8*)getDataPtr();

      // Run through my list of PDOs and pass each one the data associated with it.
      int ct = pdos.length();
      for( int i=0; i<ct; i++ )
      {
         RefObjLocker<TPDO> pdo( pdos[i].ref );
         if( pdo )
            pdo->ProcessData( datPtr, pdos[i].byteLen, 0 );
         datPtr += pdos[i].byteLen;
      }

      return 0;
   }
};

class RPDO_List: public PDO_List
{
protected:
   // Update the local buffer with data from all attached PDOs
   void InitBuffer( void )
   {
      for( int i=0; i<pdos.length(); i++ )
      {
         RefObjLocker<RPDO> pdo( pdos[i].ref );;
         if( pdo )
            pdo->LoadData( &buff[pdos[i].offset], pdos[i].byteLen );
      }
   }

public:
   RPDO_List( void ): PDO_List( 0x1C12, 0x1600, 0x0810, 0x0064, 5 ){};

   // One time init of RPDO list.  This is called when the node is first
   // attached to the network.
   const Error *Init( Node *node )
   {
      return UpdtSyncMgr( node );
   }

   // Freshen the data.  This is called just before the PDO is sent out.
   // Returns true if the PDO data was successfully updated
   bool Freshen( EtherCAT *ecat )
   {
      uint8 *ptr = buff;
      for( int i=0; i<pdos.length(); i++ )
      {
         if( !ecat->LoadPdoDat( pdos[i].ref, ptr, pdos[i].byteLen ) )
            return false;
         ptr += pdos[i].byteLen;
      }
      return true;
   }
};

/***************************************************************************/
/**
The EtherCatNodeInfo structure holds some data required by the EtherCAT network
interface which is present in every node it manages.  The contents of this
structure should be considered the private property of the EtherCAT class.
*/
/***************************************************************************/
#define SM_RXMBX    0    // Sending mailbox data to the node
#define SM_TXMBX    1    // Receiving mailbox data from the node
#define SM_RXPDO    2    // Sending PDO data to the node
#define SM_TXPDO    3    // Receiving PDO data from the node
#define SM_TOTAL    4
class EtherCatNodeInfo: public NetworkNodeInfo
{
public:
   // Node ID internally assigned by EtherCAT network
   int16 id;

   uint8 mbCount;

   // List of enabled transmit/receive PDOs
   TPDO_List tpdos;
   RPDO_List rpdos;

   // FoE transfer state info
   uint32 foePacket;
   uint32 foeErrCode;
   uint16 foeRemain;
   uint16 foeBuffSize;
   bool foeXferDone;
   char *foeErrMsg;
   uint8 *foeBuffer;

   // Supported Mailbox protocols (from EEPROM)
   uint8 mboxProtocols;

   // Identifying information from EEPROM
   NodeIdentity eepromID;

   // Sync manager base address and length
   uint16 syncBase[SM_TOTAL];
   uint16 syncLen[SM_TOTAL];
   uint8  syncCtrl[SM_TOTAL];

   uint8 *syncBuff[SM_TOTAL];

   // This mutex protects access to the mailbox
   Mutex mbxMtx;

   EtherCatNodeInfo( Node *node )
   {
      id = -1;
      mbCount = 0;
      foePacket = 0;
      foeErrCode = 0;
      foeErrMsg = 0;
      foeRemain = 0;
      foeBuffer = 0;
      foeBuffSize = 0;
      foeXferDone = false;
      mboxProtocols = 0;

      for( int i=0; i<SM_TOTAL; i++ )
      {
         syncBase[i] = 0;
         syncLen[i]  = 0;
         syncCtrl[i] = 0;
         syncBuff[i] = 0;
      }
   }

   ~EtherCatNodeInfo()
   {
      delete foeErrMsg;
      delete foeBuffer;
      for( int i=0; i<SM_TOTAL; i++ )
         delete syncBuff[i];
   }
};

CML_NAMESPACE_END()

uint8 EcatFrame::dgIndex = 0;

EtherCAT::EtherCAT( void )
{
   SetRefName( "EtherCAT" );

   xmitRef = 0;
   recvRef = 0;
   nodeAlias = 0;
   nodes = 0;
   nodeCt = 0;
   mac = 0;
   stopSem = 0;
   nextFrameIndex = 0;
   nextFrameID = 1;
   readThreadRunning = false;
   cycThreadRunning = false;
   refClkNode = -1;

   for( int i=0; i<CML_MAX_ECAT_FRAMES; i++ )
      sentFrames[i] = 0;
}

EtherCAT::~EtherCAT( void )
{
   KillRef();
   Close();
}

EtherCatNodeInfo *EtherCAT::GetEcatInfo( Node *n )
{
   return (EtherCatNodeInfo *)GetNodeInfo( n );
}

const Error *EtherCAT::Open( EtherCatHardware &hw )
{
   EtherCatSettings settings;
   return Open( hw, hw, settings );
}

const Error *EtherCAT::Open( EtherCatHardware &hw, EtherCatSettings &settings )
{
   return Open( hw, hw, settings );
}

const Error *EtherCAT::Open( EtherCatHardware &xmit, EtherCatHardware &recv )
{
   EtherCatSettings settings;
   return Open( xmit, recv, settings );
}

const Error *EtherCAT::Open( EtherCatHardware &xmit, EtherCatHardware &recv, EtherCatSettings &set )
{
   const Error *err;
   EcatFrame frame;
   uint16 i;
   BRD brd( 0, 1 );

   cml.Debug( "Attempting to open EtherCAT network\n" );

   // Make a local copy of the passed settings
   settings = set;

   xmitRef = xmit.GrabRef();
   recvRef = recv.GrabRef();

   // Open the low level Ethernet port used to transmit to the network
   err = xmit.Open();
   if( err ) goto fail;

   // Open the receive port if it's different from the transmit port
   if( xmitRef != recvRef )
   {
      err = recv.Open();
      if( err ) goto fail;
   }

   cml.Debug( "Starting EtherCAT read thread\n" );
   readThread.setPriority( settings.readThreadPriority );
   readThread.ecat = this;
   err = readThread.start();
   if( err ) goto fail;
   readThreadRunning = true;

   // Read some basic information on all the connected devices
   frame.Reset();
   frame.Add( &brd );

   err = SendFrame( &frame, 500 );
   if( err ) goto fail;
   nodeCt = brd.getADP();

   cml.Debug( "Found %d nodes on the EtherCAT network\n", nodeCt );

   nodeAlias = new int32[nodeCt];
   nodes = new uint32[nodeCt];
   for( i=0; i<nodeCt; i++ )
   {
      nodes[i] = 0;

      frame.Reset();

      // Address 0x12 holds the nodes alias (programmed into flash or from a dip switch)
      APRD alias( i, 0x12, 2 );

      // Address 0x10 holds the nodes address.  I simply set this to the node number 
      APWR addr( i, 0x10, 2, i );

      // Address 0x103 is a register that turns off the use of the alias.  We don't address
      // nodes based on the alias in CML, we always use the configured address
      APWR noAlias( i, 0x103, 1, 0 );

      frame.Add( &alias );
      frame.Add( &addr );
      frame.Add( &noAlias );

      err = SendFrame( &frame, 500 );
      if( err ) goto fail;

      if( alias.getWKT() < 1 )
      {
         cml.Debug( "  Alias register not supported for node at position %d\n", i );
         nodeAlias[i] = -1;
      }
      else
      {
         nodeAlias[i] = alias.getData16u();
         cml.Debug( "  Ecat node at position %d: alias 0x%04x\n", i, nodeAlias[i] );
      }
   }

   err = InitDistClk();
   if( err ) goto fail;

   cml.Debug( "Starting EtherCAT cycle thread\n" );
   cycleThread.setPriority( settings.cycleThreadPriority );
   cycleThread.ecat = this;
   cycleThread.start();
   cycThreadRunning = true;

   return 0;

fail:
   Close();
   cml.Warn( "EtherCAT Open failed with error %s\n", err->toString() );
   return err;
}

const Error *EtherCAT::Close( void )
{
   // Try stopping the cycle and read threads in 
   // a controlled mannor if possible
   mtx.Lock();
   Semaphore stop;
   stopSem = &stop;
   mtx.Unlock();

   if( readThreadRunning ) stop.Get( 1000 );
   if( cycThreadRunning  ) stop.Get( 1000 );

   mtx.Lock();
   stopSem = 0;
   mtx.Unlock();

   readThreadRunning = false;
   cycThreadRunning  = false;

   // Close the transmit hardware
   {
      RefObjLocker<EtherCatHardware> hw( xmitRef );
      if( hw ) hw->Close();
   }

   // Close the receive hardware
   if( xmitRef != recvRef )
   {
      RefObjLocker<EtherCatHardware> hw( recvRef );
      if( hw ) hw->Close();
   }

   // Release both references
   RefObj::ReleaseRef( xmitRef );
   RefObj::ReleaseRef( recvRef );
   xmitRef = recvRef = 0;

   // Delete data allocated during my Open call
   if( nodes )
   {
      for( int i=0; i<nodeCt; i++ )
         RefObj::ReleaseRef( nodes[i] );
      delete[] nodes;
   }
   if( nodeAlias ) delete[] nodeAlias;
   if( mac ) delete[] mac;

   nodeAlias = 0;
   nodes = 0;
   mac = 0;
   nodeCt = 0;

   nextFrameIndex = 0;
   nextFrameID = 1;

   // Remove any references to sent frames
   for( int i=0; i<CML_MAX_ECAT_FRAMES; i++ )
   {
      if( sentFrames[i] )
         RefObj::ReleaseRef( sentFrames[i] );
      sentFrames[i] = 0;
   }
   return 0;
}

const Error *EtherCAT::AttachNode( Node *n )
{
   const Error *err;

   if( !xmitRef ) return &EtherCatError::EcatNotInit;

   cml.Debug( "Adding node %d to EtherCAT network\n", n->GetNodeID() );

   // Make sure this node exists on our network.  If the node ID
   // assigned is positive, then it's a node alias.  If it's negative
   // then it's the position on the bus (-1 for first, -2 for second, etc)
   int16 id = n->GetNodeID();
   if( id < 0 )
   {
      if( -id > nodeCt )
      {
         cml.Error( "Error, can't add node at position %d.  Only %d nodes found on network\n", -id, nodeCt );
         return &EtherCatError::NodeNotFound;
      }
      id = -id-1;
   }
   else
   {
      int16 i;
      for( i=0; i<nodeCt; i++ )
      {
         if( nodeAlias[i] == id )
         {
            id = i;
            break;
         }
      }
      if( i == nodeCt )
      {
         cml.Error( "Error adding node %d to network.  No node found using that alias\n", id );
         return &EtherCatError::NodeNotFound;
      }
   }

   EtherCatNodeInfo *ni = new EtherCatNodeInfo( n );
   SetNodeInfo( n, ni );
   ni->id = id;

   // Put the node in init mode before configuring
   err = StopNode( n );
   if( err )
   {
      cml.Error( "Error stopping node %d: %s\n", n->GetNodeID(), err->toString() );
      return err;
   }

   // Read sync manager config info from the EEPROM
   err = FindSyncMgrCfg( n, false );
   if( err )
   {
      cml.Error( "Failed to find sync manager config for node %d\n", id );
      return err;
   }

   // Save the starting address of the process data sync managers.
   // These sync managers are updated as PDOs are added
   ni->tpdos.SetSyncRamAddr( ni->syncBase[SM_TXPDO] );
   ni->rpdos.SetSyncRamAddr( ni->syncBase[SM_RXPDO] );

   // Configure the mailbox sync managers
   for( int i=0; i<2; i++ )
   {
      delete ni->syncBuff[i];
      ni->syncBuff[i] = new uint8[ ni->syncLen[i] ];

      err = CfgSyncMgr( n, 0x800 + 8*i, ni->syncBase[i], ni->syncLen[i], ni->syncCtrl[i] );
      if( err ) return err;
   }

   // Disable PDO heartbeat by default
   SetNodeGuard( n, GUARDTYPE_NONE );

   // Check to make sure we are at least pre-operational.
   err = PreOpNode( n );
   if( err ) return err;

   // If this node supports CoE, disable all PDOs connected to this node.  
   if( ni->mboxProtocols & 0x04 )
   {
      err = n->sdo.Dnld8( 0x1c12, 0, (byte)0 );
      if( err ) return err;

      err = n->sdo.Dnld8( 0x1c13, 0, (byte)0 );
      if( err ) return err;
   }

   // Map the empty receive PDO list.  If no receive PDOs are added, then this default
   // setting will ensure at a least 1 byte is written to keep the heartbeat active
   ni->rpdos.Init( n );

   // Keep a reference to this node for future reference
   nodes[id] = n->GrabRef();

   return 0;
}

const Error *EtherCAT::CfgSyncMgr( Node *n, uint16 smReg, uint16 base, uint16 len, uint16 ctrl )
{
   uint16 ena = len ? 1 : 0;

   // Disable the sync manager.  It can only be updated when disabled.
   const Error *err = NodeWrite( n, smReg+6, 1, 0 );
   if( err ) return err;

   // Configure the sync manager
   byte buff[8];
   uint16_to_bytes( base, &buff[0] );
   uint16_to_bytes( len,  &buff[2] );
   uint16_to_bytes( ctrl, &buff[4] );
   uint16_to_bytes( ena,  &buff[6] );

   return NodeWrite( n, smReg, 8, buff );
}

/**
  Return the EtherCAT address assigned to this node.  When each node is added to an 
  EtherCAT network, the network object assigns it a unique address.  This address is then
  used by the master to communicate with the node over the network.
  @param n      Pointer to the node object
  @param addr   The assigned address will be returned here
  @return An error pointer on failure, or null on success.
*/
const Error *EtherCAT::GetNodeAddress( Node *n, uint16 &addr )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;
   addr = ni->id;
   return 0;
}

/**
   Read the node ID info that was pullsed from EEPROM on startup.
   This can be useful on nodes that don't support the CoE protocol
   and therefor can't use the more standard Node::GetIdentity method.
   @param n  Points to the node to access
   @param id Structure where identiy info will be returned
   @return An error pointer on failure, or null on success.
*/
const Error *EtherCAT::GetIdFromEEPROM( Node *n, NodeIdentity &id )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;
   id = ni->eepromID;
   return 0;
}

/**
   Read the EEPROM on the node to determine it's sync manager
   configuration.
   @param n    Points to the node to access
   @param boot If true, just init the mailbox sync managers for boot mode
   @return An error pointer on failure, or null on success.
*/
const Error *EtherCAT::FindSyncMgrCfg( Node *n, bool boot )
{
   const Error *err;

   cml.Debug( "Checking EEPROM settings of node %d%s\n", n->GetNodeID(), (boot ? " for boot mode" : "" ) );

   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   // Find which mailbox protocols are supported by this node
   int32 val;
   err = NodeReadEEPROM( n, 0x1c, &val );
   if( err ) return err;
   ni->mboxProtocols = val;
   cml.Debug( " - Mailbox protocols supported: 0x%02x\n", ni->mboxProtocols );

   // Read some basic ID info from EEPROM
   err = NodeReadEEPROM( n,  8, (int32*)&ni->eepromID.vendorID    ); if( err ) return err;
   err = NodeReadEEPROM( n, 10, (int32*)&ni->eepromID.productCode ); if( err ) return err;
   err = NodeReadEEPROM( n, 12, (int32*)&ni->eepromID.revision    ); if( err ) return err;
   err = NodeReadEEPROM( n, 14, (int32*)&ni->eepromID.serial      ); if( err ) return err;
   cml.Debug( " - Device ID info: Vendor: 0x%08x, Prouct: 0x%08x, Revision: 0x%08x, Serial Number: 0x%08x\n", 
                  ni->eepromID.vendorID, ni->eepromID.productCode, ni->eepromID.revision, ni->eepromID.serial );

   // Read the transmit & receive mailbox config info from EEPROM.
   int32 rxMbx, txMbx;
   if( boot )
   {
      err = NodeReadEEPROM( n, 0x14, &rxMbx );
      if( err ) return err;

      err = NodeReadEEPROM( n, 0x16, &txMbx );
      if( err ) return err;
   }
   else
   {
      err = NodeReadEEPROM( n, 0x18, &rxMbx );
      if( err ) return err;

      err = NodeReadEEPROM( n, 0x1a, &txMbx );
      if( err ) return err;
   }

   // Standard receive mailbox offset & size
   ni->syncBase[SM_RXMBX] = rxMbx;
   ni->syncBase[SM_TXMBX] = txMbx;
   ni->syncBase[SM_RXPDO] = 0;
   ni->syncBase[SM_TXPDO] = 0;

   ni->syncLen [SM_RXMBX] = rxMbx >> 16;
   ni->syncLen [SM_TXMBX] = txMbx >> 16;
   ni->syncLen [SM_RXPDO] = 0;
   ni->syncLen [SM_TXPDO] = 0;

   ni->syncCtrl[SM_RXMBX] = 0x26;
   ni->syncCtrl[SM_TXMBX] = 0x22;
   ni->syncCtrl[SM_RXPDO] = 0x64;
   ni->syncCtrl[SM_TXPDO] = 0x20;

   cml.Debug( " - Receive mailbox start: 0x%04x, size: 0x%04x\n", ni->syncBase[SM_RXMBX], ni->syncLen [SM_RXMBX] );
   cml.Debug( " - Transmit mailbox start: 0x%04x, size: 0x%04x\n", ni->syncBase[SM_TXMBX], ni->syncLen [SM_TXMBX] );

   // For boot mode, that's all I check
   if( boot ) return 0;

   // Read through the various config categories in the EEPROM until I
   // find the sync manager configuration.  Quit when I find it or get
   // to the end of the categories.
   int32 addr = 0x40;
   uint16 cat, len;
   while( addr < 0x1000 )
   {
      err = NodeReadEEPROM( n, addr, &val );
      if( err ) return err;
      addr += 2;

      cat = val & 0xffff;
      len = val>>16;

      // If this is the end of the EEPROM category info, just quit
      if( cat == 0xffff ) return 0;

      // If we hit the sync manager info, quit
      if( cat == 41 )
         break;

      // If length is zero, just quit.  The EEPROM seems to have invalid
      // data in it.
      if( !len ) return 0;

      // Skip to the next category
      addr += len;
   }

   // Make sure I found the sync manager info.  If not just quit
   if( cat != 41 ) return 0;

   // Each sync manager has 4 words of config info
   int ct = len/4;
   if( ct > 4 ) ct = 4;
   cml.Debug( " - Reading sync manager configuration data from EEPROM.  %d total SM available, reading %d\n", len/4, ct );
   for( int i=0; i<ct; i++ )
   {
      int32 a, b;
      err = NodeReadEEPROM( n, addr, &a );
      if( err ) return err;
      addr += 2;

      err = NodeReadEEPROM( n, addr, &b );
      if( err ) return err;
      addr += 2;

      uint16 start = a;
      uint16 len  = a>>16;
      uint8 ctrl = b;
      uint8 ena  = b>>16;
      uint8 type = b>>24;

      cml.Debug( " - SM %d, start: 0x%04x, len: 0x%04x, ctrl: 0x%02x, enable: 0x%02x, type: 0x%02x\n", i, start, len, ctrl, ena, type );
      if( (type==0) || (type>4) )
         cml.Debug( "     Ignoring sync manager due to type code\n" );
      else
      {
         ni->syncBase[type-1] = start;
         ni->syncLen [type-1] = len;
      }
   }

   return 0;
}

const Error *EtherCAT::DetachNode( Node *n )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   // Keep the cyclic thread from accessing this node while I remove it
   cyclicMutex.Lock();

   if( nodes && (ni->id < nodeCt) )
   {
      RefObj::ReleaseRef( nodes[ni->id] );
      nodes[ni->id] = 0;
   }

   delete ni;
   SetNodeInfo( n, 0 );
   cyclicMutex.Unlock();

   return 0;
}

// Reset a node on an EtherCAT network.
// Note that this will most likely bring the network down while the node is resetting,
// so it's not generally a good idea to reset nodes on an EtherCAT network.
const Error *EtherCAT::ResetNode( Node *n )
{
   const Error *err;
   char buff[3] = { 'R', 'E', 'S' };

   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   cml.Debug( "Resetting node %d\n", n->GetNodeID() );

   cyclicMutex.Lock();
   int i;
   for( i=0; i<3; i++ )
   {
      err = NodeWrite( n, 0x40, 1, &buff[i] );
      if( err ) break;
   }
   cyclicMutex.Unlock();
   if( err ) return err;

   // Delay briefly to let the node fully reset
   Thread::sleep(10);

   // Remove the node 
   err = DetachNode( n ); 
   if( err ) return err;

   // Try doing a broadcast read multiple times until we see the 
   // expected number of nodes on the network.
   for( i=0; i<200; i++ )
   {
      EcatFrame frame;
      BRD brd( 0, 1 );

      frame.Reset();
      frame.Add( &brd );

      if( SendFrame( &frame, 100 ) )
         continue;

      cml.Debug( "Broadcast read after reset shows %d of %d nodes on try %d\n", brd.getADP(), nodeCt, i );
      if( brd.getADP() >= nodeCt )
         break;
   }

   // Re-attach the node to this network 
   return AttachNode( n );
}

const Error *EtherCAT::ResetComm( Node *n ){ return 0; }

void EtherCAT::EcatCycleThread::run( void ){ ecat->CycleThreadFunc(); }
void EtherCAT::EcatReadThread::run( void ){ ecat->ReadThreadFunc(); }

const Error *EtherCAT::PdoDisable( Node *node, uint16 slot, PDO *pdo )
{
   return PdoDisable( node, slot, pdo->IsTxPDO() );
}

int32 EtherCAT::maxSdoToNode( Node *n )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return 0;
   return ni->syncLen[SM_RXMBX];
}

int32 EtherCAT::maxSdoFromNode( Node *n )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return 0;
   return ni->syncLen[SM_TXMBX];
}


/**
Set the period of the SYNC0 signal used on nodes with a distributed clock.
This also starts generation of the SYNC0 signal.

@param n  The node to modify
@param ns The period in nanoseconds.
@return An error object, or NULL on success
*/
const Error *EtherCAT::SetSync0Period( Node *n, uint32 ns )
{
   // Disable SYNC0 generation
   const Error *err = NodeWrite( n, 0x980, 2, 0 );
   if( err ) return err;

   // Set the SYNC0 period
   err = NodeWrite( n, 0x9A0, 4, &ns );
   if( err || !ns ) return err;

   // Get the current system time on this node
   int64 sysTime, nxtTime;
   err = NodeRead( n, 0x910, 8, &sysTime );
   if( err ) return err;

   // Round the system time off to an even multiple of this period
   int64 start = sysTime - sysTime % ns;

   // Find a number of periods to add to this starting time.
   // I shoot for about 10ms worth initially
   int32 ct = 10000000 / ns;
   if( ct < 1 ) ct = 1;
   start += ct * ns;

   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   // Try starting the SYNC0 pulses and make sure I wasn't too late
   // Each try I'll increase the period by around 10ms worth.
   int i;
   for( i=0; i<20; i++ )
   {
      start += i * ct * ns;

      EcatFrame frame;
      FPWR startTime( ni->id, 0x990, 8, (uint8*)&start );
      frame.Add( &startTime );

      FPWR cfg( ni->id, 0x980, 2, 0x0300 );
      frame.Add( &cfg );

      err = SendFrame( &frame, 500 );
      if( err ) return err;

      // Now, read the system time and SYNC0 time.  
      frame.Reset();

      FPRD nxt( ni->id, 0x990, 8 );
      frame.Add( &nxt );

      FPRD sys( ni->id, 0x910, 8 );
      frame.Add( &sys );

      err = SendFrame( &frame, 500 );
      if( err ) return err;

      nxt.getData( &nxtTime, 8 );
      sys.getData( &sysTime, 8 );

//cml.Debug( "Sys: %lld, next: %lld, diff: %.3lfms\n", sysTime, nxtTime, (nxtTime-sysTime) * 0.000001 );

      // If the system time is > the next SYNC0 time, then I didn't make it in time.
      // Try again with a longer delay
      if( sysTime < nxtTime )
         break;
   }

   // Return an error if we weren't successful in setting the sync0 time.
   if( i == 20 )
      return &EtherCatError::Sync0Config;

   // Successfully set the SYNC0 time.  In doing this I added a few milliseconds of additional padding to the first
   // SYNC0 pulse.  I need to wait here for that pulse to occur before I can continue.  Otherwise, nodes may generate
   // an error when going operational.
   for( i=0; i<40; i++ )
   {
      // Read system time and next sync0 time again
      EcatFrame frame;
      FPRD nxt( ni->id, 0x990, 8 );
      frame.Add( &nxt );
      FPRD sys( ni->id, 0x910, 8 );
      frame.Add( &sys );

      err = SendFrame( &frame, 500 );
      if( err ) return err;

      int64 sync2, sys2;
      nxt.getData( &sync2, 8 );
      sys.getData( &sys2, 8 );

//cml.Debug( "Checking Sys: %lld, next: %lld, diff: %.3lfms\n", sys2, sync2, (sync2-sys2) * 0.000001 );

      // If the sync0 time changed, then I'm done.
      if( sync2 != nxtTime )
         return 0;

      // If the system time somehow passed the sync0 time, then the setup failed for some strange reason
      if( sys2 > sync2 )
         break;

      // Sleep here.  This loop will timeout after about 40ms if the SYNC0 isn't working correctly
      Thread::sleep(1);
   }

   return &EtherCatError::Sync0Config;
}

/***************************************************************************/
/**
Configure the heartbeat protocol for an EtherCAT node.  This sets the heartbeat
timeout used for process data on the EtherCAT node.

@param n       The node to configure heartbeat on
@param type    The type of node guarding to configure.
@param timeout A timeout (milliseconds) to use for this node guarding protocol.
               If not specified, this parameter defaults to 200 milliseconds.
@param life    This parameter is not used under EtherCAT.
@return        An error object, or NULL on success
 */
/***************************************************************************/
const Error *EtherCAT::SetNodeGuard( Node *n, GuardProtocol type, Timeout timeout, uint8 life )
{
   // Set the heartbeat divider to 1ms (25000 counts)
   uint16 x = 24998;

   const Error *err = NodeWrite( n, 0x400, 2, &x );
   if( err ) return err;

   // If the type is set to none, disable the heartbeat
   if( type == GUARDTYPE_NONE )
      x = 0;
   else if( timeout < 65536 )
      x = (uint16)timeout;
   else
      x = 65535;

   return NodeWrite( n, 0x420, 2, &x );
}

const Error *EtherCAT::PdoDisable( Node *node, uint16 slot, bool isTxPDO )
{
   EtherCatNodeInfo *ni = GetEcatInfo( node );
   if( !ni ) return &EtherCatError::NodeNotInit;

   const Error *err;
   if( isTxPDO )
      err = ni->tpdos.RemPDO( node, slot );
   else
      err = ni->rpdos.RemPDO( node, slot );

   return err;
}

const Error *EtherCAT::PdoEnable( Node *node, uint16 slot, PDO *pdo )
{
   EtherCatNodeInfo *ni = GetEcatInfo( node );
   if( !ni ) return &EtherCatError::NodeNotInit;

   const Error *err;
   if( pdo->IsTxPDO() )
      err = ni->tpdos.AddPDO( node, pdo, slot );
   else
   {
      err = ni->rpdos.AddPDO( node, pdo, slot );
      // This just fills a buffer with the current PDO data 
      if( !err ) XmitPDO( pdo, 0 );
   }

   return err;
}

/**
Add a datagram to the frame.  If the frame is too large to add the datagram, then
first send the frame, reset it and add the datagram.
@param frame The frame
@param dg  The datagram
@return An error pointer or NULL on success
*/
const Error *EtherCAT::AddToFrame( EcatFrame *frame, EcatDgram *dg )
{
   // Add the datagram to the frame
   const Error *err = frame->Add( dg );
   if( err != &EtherCatError::DatagramWontFit )
      return err;

   // If the frame was too full to add that datagram, just send it
   err = SendFrame( frame, 500 );
   if( err ) return err;
      
   // Now, reset the frame and add the datagram
   frame->Reset();
   return frame->Add( dg );
}

/*
 * EtherCAT network read thread.
 *
 * This thread constantly waits for messages to be received over the Ethernet
 * port and processes them as they come.
 *
 * I encode an object reference number in each frame I send out.  This reference
 * is used to identify the frames as they are returned.
 */
void EtherCAT::ReadThreadFunc( void )
{
   cml.Debug( "EtherCAT::ReadThreadFunc started\n" );
   uint8 buff[MAX_ECAT_FRAME];
   EtherCatHardware *hwPtr;
   const Error *err;

   while( !stopSem )
   {
      // Get a pointer to the receiving Ethernet hardware
      hwPtr = (EtherCatHardware *)RefObj::LockRef( recvRef );

      // If that fails, just sleep briefly and try again
      if( !hwPtr ) 
      {
         cml.Debug( "EtherCAT::ReadThreadFunc - unable to lock receiver hardware\n" );
         Thread::sleep(10);
         continue;
      }

      uint16 count = MAX_ECAT_FRAME;
      err = hwPtr->RecvPacket( buff, &count, 50 );
      hwPtr->UnlockRef();

      if( err )
      {
         cml.Debug( "EtherCAT::ReadThreadFunc - error reading EtherCAT frame: %s\n", err->toString() );
         continue;
      }

      // Try to grab an index and ID number stored in the beginning 
      // of the frame.
      EcatFrame *frame = FindFrame( count, buff );
      if( !frame )
         continue;

      // Process the frame
      err = frame->Process( buff, count );
      frame->UnlockRef();
   }

   mtx.Lock();
   cml.Debug( "EtherCAT::ReadThreadFunc stopping %p\n", stopSem );
   if( stopSem )
      stopSem->Put();
   mtx.Unlock();
}

/**
  This thread runs in the background and is responsible for polling the 
  process data of all devices on the network periodically.
*/
void EtherCAT::CycleThreadFunc( void )
{
   cml.Debug( "EtherCAT::CycleThreadFunc started\n" );
   EcatFrame frame;

   // Run until a semaphore is posted telling us it's time to shut down
   while( !stopSem )
   {
      Thread::sleep(settings.cyclePeriod);
      if( stopSem ) break;

      frame.Reset();

      ARMW dcTime( refClkNode, 0x910, 8 );
      if( refClkNode >= 0 )
         frame.Add( &dcTime );

      cyclicMutex.Lock();

      // FIXME - I should also poll the AL status of the nodes to watch for errors
      for( int n=0; n<nodeCt; n++ )
      {
         if( !nodes[n] ) continue;

         // Convert my reference to a node pointer
         Node *node = (Node*)RefObj::LockRef( nodes[n] );
         if( !node ) continue;

         // Check the node state.  Process data only works in safe-op or op mode.
         NodeState state = node->GetState();
         if( (state!=NODESTATE_OPERATIONAL) && (state!=NODESTATE_SAFE_OP) )
         {
            node->UnlockRef();
            continue;
         }

         EtherCatNodeInfo *ni = GetEcatInfo( node );
         if( !ni )
         {
            node->UnlockRef();
            continue;
         }

         // Add a datagram used to send PDO data to node
         if( ni->rpdos.isEnabled() && ni->rpdos.Freshen( this ) )
            AddToFrame( &frame, &ni->rpdos );

         // Add a datagram used to read PDO data from node
         if( ni->tpdos.isEnabled() )
            AddToFrame( &frame, &ni->tpdos );

         node->UnlockRef();
      }

      // If any PDOs were added to the frame, process it
      if( !frame.IsEmpty() ) SendFrame( &frame, 50, 0 );

      // Toggle a bit in the event map.
      cyclicUpdate.setMask( cyclicUpdate.getMask() ^ 1 );
      cyclicMutex.Unlock();
   }

   mtx.Lock();
   cml.Debug( "EtherCAT::CycleThreadFunc stopping %p\n", stopSem );
   if( stopSem )
      stopSem->Put();
   mtx.Unlock();
}

/// Wait for the cyclic thread to update.
/// @param to Max time to wait before returning an error
/// @return An error object on failure, or NULL on success.
const Error *EtherCAT::WaitCycleUpdate( Timeout to )
{
   if( cyclicUpdate.getMask() & 1 )
   {
      EventNone event(1);
      return event.Wait( cyclicUpdate, to );
   }
   else
   {
      EventAll event(1);
      return event.Wait( cyclicUpdate, to );
   }
}

// Store a unique frame ID value in the passed frame and hold on to
// a reference to the frame so I can find it later.
void EtherCAT::SaveFrameRef( EcatFrame *frame )
{
   uint32 old, ref = frame->GrabRef();

   MutexLocker ml( mtx );
   frame->SetFrameID( nextFrameIndex+1, nextFrameID++ );

   old = sentFrames[nextFrameIndex];
   sentFrames[nextFrameIndex] = ref;

   if( ++nextFrameIndex >= CML_MAX_ECAT_FRAMES )
      nextFrameIndex = 0;

   if( old )
      RefObj::ReleaseRef( old );

   return;
}

// Try to locate an EtherCAT frame object that corresponds
// to a raw block of data received over the network.
EcatFrame *EtherCAT::FindFrame( uint8 index, uint32 id )
{
   // If the index isn't valid then retrun null
   if( index < 1 || index > CML_MAX_ECAT_FRAMES )
   {
      cml.Debug( "Error finding frame %d 0x%08x, invalid index\n", index, id );
      return 0;
   }

   // Check my list of sent frames to see if the one at
   // this index location matches the received frame.
   MutexLocker ml( mtx );
   uint32 ref = sentFrames[index-1];

   if( !ref )
   {
      cml.Debug( "Error finding frame %d 0x%08x, saved ref is zero\n", index, id );
      return 0;
   }

   EcatFrame *frame = (EcatFrame *)RefObj::LockRef( ref );
   if( !frame ) return 0;

   if( frame->GetFrameID() != id )
   {
      cml.Debug( "Error finding frame %d 0x%08x, saved id is 0x%08x\n", index, id, frame->GetFrameID() );
      frame->UnlockRef();
      return 0;
   }

   RefObj::ReleaseRef( ref );
   sentFrames[index-1] = 0;
   return frame;
}

// Find a frame by parsing the raw Ethernet data received
EcatFrame *EtherCAT::FindFrame( uint16 len, uint8 *buff )
{
   uint32 id;
   uint8 index = EcatFrame::FindFrameID( len, buff, id );
   if( !index ) return 0;

   return FindFrame( index, id );
}

// Remove the passed frame from my local tables.
void EtherCAT::ReleaseFrameRef( EcatFrame *frame )
{
   uint8 index = frame->GetFrameIndex();
   uint32 id = frame->GetFrameID();

   frame = FindFrame( index, id );
   if( frame )
      frame->UnlockRef();
}

const Error *EtherCAT::SendFrame( EcatFrame *frame, Timeout timeout, int maxRetry )
{
   const Error *err;
   
   do
   {
      // Keep a local reference to this frame so I can find it later
      SaveFrameRef( frame );

      EtherCatHardware *hwPtr = (EtherCatHardware *)RefObj::LockRef( xmitRef );
      if( !hwPtr )
      {
         ReleaseFrameRef( frame );
         cml.Error( "Error Dereferencing transmit hardware\n" );
         return &EtherCatError::EcatNotInit;
      }

      // Send the packet
      err = hwPtr->SendPacket( frame->getBuff(), frame->getSize() );
      hwPtr->UnlockRef();

      if( err )
      {
         ReleaseFrameRef( frame );
         cml.Error( "Error Sending EtherCAT packet: %s\n", err->toString() );
         return err;
      }

      // If timeout was non-zero, wait for response to be received & 
      // processed before returning
      if( timeout )
      {
         Timeout to = timeout;
         if( maxRetry && (timeout>20) )
            to = 20;

         err = frame->WaitResponse( to );
      }

      // If I received a response, just return.  There's no need to release the frame 
      // reference, that was done by the read thread.
      if( !err )
         return 0;

      ReleaseFrameRef( frame );

   } while( maxRetry-- );

   cml.Error( "Error Waiting on EtherCAT response: %s\n", err->toString() );
   return err;
}

const Error *EtherCAT::NodeRead( Node *n, int16 addr, int16 len, void *ptr )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   uint8 *buff = (uint8*)ptr;

   EcatFrame frame;
   FPRD dgram( ni->id, addr, len );
   frame.Add( &dgram );

   const Error *err = SendFrame( &frame, 500 );
   if( err ) return err;

   // TODO: should confirm the number of read bytes
   dgram.getData( buff, len );
   return 0;
}

const Error *EtherCAT::NodeWrite( Node *n, int16 addr, int16 len, int32 dat )
{
   uint8 buff[4];

   if( len < 1 || len > 4 ) return &CanOpenError::BadParam;

   int32_to_bytes( dat, buff );
   return NodeWrite( n, addr, len, buff );
}

const Error *EtherCAT::NodeWrite( Node *n, int16 addr, int16 len, void *ptr )
{
   uint8 *buff = (uint8*)ptr;
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   EcatFrame frame;
   FPWR dgram( ni->id, addr, len, buff );
   frame.Add( &dgram );

   return SendFrame( &frame, 500 );
}

const Error *EtherCAT::PollChange( Node *n, int16 addr, int16 len, void *buff )
{
   const Error *err;
   uint8 local[64];

   if( len < 1 || len > (int16)sizeof(local) ) return &CanOpenError::BadParam;

   memcpy( local, buff, len );

   int i;
   for( i=0; i<50; i++ )
   {
      err = NodeRead( n, addr, len, buff );
      if( err ) return err;

      if( memcmp( buff, local, len ) )
         return 0;

      Thread::sleep(3);
   }
   return &ThreadError::Timeout;
}

const Error *EtherCAT::WaitEEPROM( Node *n, Timeout to, uint16 &stat )
{
   // Wait for the EEPROM to be idle.  This should normally be the 
   // case when we start a read.
   uint32 start = Thread::getTimeMS();

   while( true )
   {
      const Error *err = NodeRead( n, 0x502, 2, &stat );
      if( err ) return err;

      if( !(stat & 0x8000) ) return 0;

      uint32 now = Thread::getTimeMS();
      if( now - start > to )
      {
         cml.Debug( "Timeout waiting for EEPROM to be idle on node %d\n", n->GetNodeID() );
         return &ThreadError::Timeout;
      }
      Thread::sleep(1);
   }
   return 0;
}

// Read 32-bits from the nodes EEPROM
const Error *EtherCAT::NodeReadEEPROM( Node *n, int32 addr, int32 *ret )
{
   const char *msg;
   const Error *err;
   int retry;

   // Wait for the EEPROM to be idle.  This should normally be the 
   // case when we start a read.
   uint16 stat;
   err = WaitEEPROM( n, 100, stat );
   if( err ){ msg = "waiting for EEPROM to go idle"; goto fail; }

   for( retry=0; retry<3; retry++ )
   {
      // If any error bit is set in the status register, clear it by writing a 0 command
      if( stat & 0x6000 )
      {
         err = NodeWrite( n, 0x502, 2, 0 );
         if( err ){ msg="clearing EEPROM error bits"; goto fail; }
      }

      // Write the address that we are going to read
      err = NodeWrite( n, 0x504, 4, addr );
      if( err ){ msg="writing EEPROM address"; goto fail; }

      // Start the EEPROM read
      err = NodeWrite( n, 0x502, 2, 0x0100 );
      if( err ){ msg="starting EEPROM read"; goto fail; }

      // Wait for the busy bit to clear
      err = WaitEEPROM( n, 100, stat );
      if( err ){ msg="waiting on EEPROM"; goto fail; }

      // If no error bits were set, the read was successful
      if( !(stat & 0x6000 ) )
         break;
   }

   // Read the data value and return it.
   err = NodeRead( n, 0x508, 4, ret );
   if( err ){ msg="Reading EEPROM value"; goto fail; }
   return 0;

fail:
   cml.Error( "Failed to read EEPROM address 0x%04x of node %d\n"
              "While %s received error %s\n", addr, n->GetNodeID(), msg, err->toString() );
   return err;
}

// Convert from EtherCAT states to my simpler set of node states
static NodeState FindNodeState( uint ecatState )
{
   switch( ecatState & 0x0F )
   {
      case 1:  return NODESTATE_STOPPED;      // Init state
      case 2:  return NODESTATE_PRE_OP;       // Pre-op state
      case 3:  return NODESTATE_BOOT;         // Boot state
      case 4:  return NODESTATE_PRE_OP;       // Safe-op state
      case 8:  return NODESTATE_OPERATIONAL;  // Operational state
      default: return NODESTATE_INVALID;
   }
}

// This table identifies the legal sequence of node state changes
// for all legal state codes.  It also includes values for illegal 
// codes, these always move to the init state.
static const int nextState[9][9] =
{ 
   // To states                    // From states
   { 1, 1, 1, 1, 1, 1, 1, 1, 1 },  // 0 - invalid
   { 1, 1, 2, 3, 2, 1, 1, 1, 2 },  // 1 - init
   { 1, 1, 2, 1, 4, 1, 1, 1, 4 },  // 2 - pre-op
   { 1, 1, 1, 3, 1, 1, 1, 1, 1 },  // 3 - boot
   { 1, 1, 2, 1, 4, 1, 1, 1, 8 },  // 4 - safe-op
   { 1, 1, 1, 1, 1, 1, 1, 1, 1 },  // 5 - invalid
   { 1, 1, 1, 1, 1, 1, 1, 1, 1 },  // 6 - invalid
   { 1, 1, 1, 1, 1, 1, 1, 1, 1 },  // 7 - invalid
   { 1, 1, 2, 1, 4, 1, 1, 1, 8 },  // 8 - operational
};

// Set the application layer state for this node
const Error *EtherCAT::SetNodeAlState( Node *n, uint16 state )
{
   const Error *err;
   uint16 x;

   // Make sure we're moving to a legal state
   CML_ASSERT( (state >= 1) && ((state <= 4) || (state==8)) );

   cml.Debug( "Attempting to change node %d state to 0x%04x\n", n->GetNodeID(), state );

   // Read the current state
   err = NodeRead( n, 0x130, 2, &x );
   if( err ) goto fail;

   cml.Debug( "Starting mode for node %d was 0x%04x\n", n->GetNodeID(), x );

   // Clear any pending error first
   if( x & 0x10 )
   {
      cml.Debug( "Writing node %d state value 0x%04x\n", n->GetNodeID(), x );
      err = NodeWrite( n, 0x120, 2, x );
      if( err ) goto fail;

      err = PollChange( n, 0x130, 2, &x );
      if( err ) goto fail;
 
      if( x > 8 )
      {
         uint16 code;
         NodeRead( n, 0x134, 2, &code );
         cml.Error( "Node %d failed to clear state error.  Returned status was 0x%04x, code 0x%04x\n", n->GetNodeID(), x, code );
         return &EtherCatError::NodeStateChange;
      }
   }

   // Step through states until I get to the desired final state
   while( 1 )
   {
      n->SetState( FindNodeState(x) );

      // Just return if the mode is correct
      if( x == state ) return 0;

      // Find the next state 
      uint16 next = nextState[x][state];

      cml.Debug( "Setting node %d state to %d\n", n->GetNodeID(), next );
      err = NodeWrite( n, 0x120, 2, next );
      if( err ) goto fail;

      err = PollChange( n, 0x130, 2, &x );
      if( err ) goto fail;

      if( x & 0x10 )
      {
         NodeRead( n, 0x134, 2, &x );
         cml.Error( "Node %d error changing to state 0x%04x, error code: 0x%04x\n", n->GetNodeID(), state, x );
         return &EtherCatError::NodeStateChange;
      }

      if( x != next )
      {
         cml.Error( "Node %d error changing to state 0x%04x, Returned state was 0x%04x\n", n->GetNodeID(), next, x );
         return &EtherCatError::NodeStateChange;
      }
   }

fail:
   cml.Error( "Failed to change node %d to state 0x%04x.  Error: %s\n", n->GetNodeID(), state, err->toString() );
   return err;
}

const Error *EtherCAT::PreOpNode( Node *n )
{
   return SetNodeAlState( n, 2 );
}

const Error *EtherCAT::StopNode( Node *n )
{
   return SetNodeAlState( n, 1 );
}

const Error *EtherCAT::StartNode( Node *n )
{
   return SetNodeAlState( n, 8 );
}

const Error *EtherCAT::BootModeNode( Node *n )
{
   return SetNodeAlState( n, 3 );
}

const Error *EtherCAT::XmitSDO( Node *n, uint8 *data, uint16 len, uint16 *ret, Timeout timeout )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   uint8 *send = ni->syncBuff[ SM_RXMBX ];
   uint8 *resp = ni->syncBuff[ SM_TXMBX ];

   // make sure len isn't too big for my mailbox
   if( len > ni->syncLen[ SM_RXMBX ] - 8 )
      return &CanOpenError::BadParam;

   // Initialize the mailbox header in the first 8 bytes of my buffer
   MutexLocker ml( ni->mbxMtx );

   // data length
   int16_to_bytes( len+2, send );

   // master address and priority are not used
   send[2] = send[3] = send[4] = 0;

   // type (CoE)
   send[5] = 3;

   // CoE message type (SDO request)
   send[6] = 0x00;
   send[7] = 0x20;

   // SDO data
   memcpy( &send[8], data, len );

   // Do the actual transfer
   uint16 retLen;
   const Error *err = MailboxTransfer( n, len+8, &retLen, timeout );
   if( err ) return err;

   // Copy just the data portion of the mailbox.  This is everything 
   // after the 8-byte header
   if( retLen < 8 )
      retLen = 0;
   else
      retLen -= 8;
   *ret = retLen;

   memcpy( data, &resp[8], retLen );
   return 0;
}

/**
  Write a block of data to the mailbox of an EtherCAT node and wait for a response.
  Note that the mailbox mutex should be held when this is called.

  @param n     The node to access.
  @param len   The number of bytes of data to send
  @param ret   returns the actual number of bytes returned.
  @param timeout The maximum time to wait for a response.
  @return An error pointer on failure, or null on success.
*/
const Error *EtherCAT::MailboxTransfer( Node *n, uint16 len, uint16 *ret, Timeout timeout )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   uint8 *send = ni->syncBuff[ SM_RXMBX ];
   uint8 *resp = ni->syncBuff[ SM_TXMBX ];

   // make sure len isn't too big for my mailbox
   if( (len > ni->syncLen[SM_RXMBX]) || (len<6) )
      return &CanOpenError::BadParam;

   // Update counter. Do this before adding to data - counter of 0 is reserved
   ni->mbCount += 0x10;
   if( ni->mbCount > 0x70 )
      ni->mbCount = 0x10;

   // Add mailbox counter to data
   send[5] &= 0x0F;
   send[5] |= ni->mbCount;

   // Write the mailbox data.
   // Loop as long as resp and send values for mbCount are the same
   EcatFrame frame;

   // Configured address physical write (FPWR)
   FPWR wdat( ni->id, ni->syncBase[ SM_RXMBX ], len, send );
   FPWR wend( ni->id, ni->syncBase[ SM_RXMBX ]+ni->syncLen[SM_RXMBX]-1, 1, 0 ); 
   frame.Add( &wdat );

   if( len != ni->syncLen[ SM_RXMBX ] )
      frame.Add( &wend );

   // Write my request to the mailbox on the frame.  I make multiple write requests if the 
   // working counter isn't getting updated
   for( int i=0; i<10; i++ )
   {
      const Error *err = SendFrame( &frame, 100 );
      if( err ) return err;

      if( wdat.getWKT() > 0 )
         break;
      cml.Debug( "Writing to mailbox of node %d, wkt didn't get updated.\n", ni->id );
   }

   // Now poll the mailbox until I receive a response from the drive
   // Configured address physical read (FPRD)
   FPRD rdat( ni->id, ni->syncBase[ SM_TXMBX ], ni->syncLen[ SM_TXMBX ] );

   uint32 startTime = Thread::getTimeMS();

   // If we are using a short poll time, then wait here before requesting the first response.
   // This increases the odds of getting a valid response on the first request.
#ifdef CML_ALLOW_FLOATING_POINT
   Thread::sleep(SHORT_DELAY);
#endif

   while(true)
   {
      frame.Reset();
      frame.Add( &rdat );

      const Error *err = SendFrame( &frame, 100 );
      if( err ) return err;

      // If the working counter is not updated I'll continue to poll mailbox until I get a response
      // or timeout
      if( !rdat.getWKT() )
      {
         if( (Thread::getTimeMS() - startTime) >= timeout )
            break;

         Thread::sleep(SHORT_DELAY);
         continue;
      }

      // The first two bytes give the number of mailbox data bytes.
      // I'll return this much data along with the 6 bytes of header info
      uint16 retLen = bytes_to_int16( (uint8*)rdat.getDataPtr() ) + 6;
      if( retLen > ni->syncLen[ SM_TXMBX ] )
      {
         cml.Error( "Node %d mailbox read returned illegal lenght(%d).  Truncated to fit in buffer\n", ni->id, retLen );
         retLen = ni->syncLen[ SM_TXMBX ];
      }

      memcpy( resp, rdat.getDataPtr(), retLen );
      *ret = retLen;

      // If the returned mailbox data contained an error code, return an appropriate error object
      if( (retLen >= 6) && !(resp[5]&0x0f) )
      {
         if( retLen < 10 )
         {
            cml.Error( "Node %d mailbox read too short (%d)\n", ni->id, retLen );
            return &EtherCatError::MboxError;
         }

         int16 err = bytes_to_int16( &resp[8] );
         switch( err )
         {
            case 1:  return &EtherCatError::MboxSyntax;
            case 2:  return &EtherCatError::MboxProtocol;
            case 3:  return &EtherCatError::MboxChannel;
            case 4:  return &EtherCatError::MboxService;
            case 5:  return &EtherCatError::MboxHeader;
            case 6:  return &EtherCatError::MboxTooShort;
            case 7:  return &EtherCatError::MboxMemory;
            case 8:  return &EtherCatError::MboxSize;
            default: 
               cml.Error( "Node %d mailbox read returned an unknown error code (%d)\n", ni->id, err );
               return &EtherCatError::MboxError;
         }
      }

      return 0;
   }
   return &ThreadError::Timeout;
}

const Error *EtherCAT::XmitPDO( PDO *pdo, Timeout timeout )
{
   // We don't support remote requests over EtherCAT.
   if( pdo->IsTxPDO() )
      return 0;

   // Find the size of the PDO data in bytes.  Make sure this is
   // consistent with the buffer in the PDO object.
   int byteSz = (pdo->bitCt+7) / 8;
   uint8 *newBuff = 0;
   if( byteSz != pdo->netBuffLen )
      newBuff = new uint8[byteSz];

   // Lock a mutex which prevents the cyclic thread from running while
   // the PDO buffer is being updated.  This ensures that the PDO data
   // sent out is consistent.
   MutexLocker ml( cyclicMutex );

   if( newBuff )
   {
      delete [] pdo->netBuff;
      pdo->netBuff = newBuff;
      pdo->netBuffLen = byteSz;
   }

   // Load data into my buffer
   ((RPDO*)pdo)->LoadData( pdo->netBuff, pdo->netBuffLen );
   return 0;
}

bool EtherCAT::LoadPdoDat( uint32 ref, uint8 *buff, int max )
{
   RefObjLocker<RPDO> pdo( ref );
   if( !pdo ) return false;
   int ct = pdo->netBuffLen;
   if( ct > max ) ct = max;

   memcpy( buff, pdo->netBuff, ct );
   if( ct < max )
      memset( &buff[ct], 0, max-ct );
   return true;
}

// Do a FoE download.  This fills in a few static bytes of the header and sends the mailbox 
// message.  It resends multiple times if a busy response is received.
// Note that the mailbox mutex should be held when this is called.
const Error *EtherCAT::FoE_Dnld( Node *n, uint16 len, Timeout to )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   uint8 *send = ni->syncBuff[ SM_RXMBX ];
   uint8 *resp = ni->syncBuff[ SM_TXMBX ];

   // Initialize the FoE header packet

   // The length of the mailbox service data is the length of the complete 
   // transfer, less 6 bytes for the mailbox header.
   int16_to_bytes( len-6, send );

   // master address, priority and reserved byte are all zero
   send[2] = send[3] = send[4] = send[7] = 0;

   // type (FoE)
   send[5] = 4;

   // send[6] holds the opcode which should have been filled in by the calling function

   // Try sending the FoE data until I get an ACK or error response
   // If I get a busy response, I'll keep trying until I time out
   uint32 startTime = Thread::getTimeMS();

   do
   {
      uint16 ret;
      const Error *err = MailboxTransfer( n, len, &ret, to );
      if( err ) return err;

      // Return a generic error if the mailbox response isn't a valid
      // FoE packet.
      if( (ret < 8) || ((resp[5]&0x0F) != 4) )
         return &EtherCatError::FoEformat;

      // Check the FoE opcode in the response.
      switch( resp[6] )
      {
         // Busy response.  Delay for a few milliseconds and try again
         case 6:
            Thread::sleep(10);
            continue;

            // Error response
         case 5:
         {
            if( ret < 12 ) return &EtherCatError::FoEformat;
            ni->foeErrCode = bytes_to_uint32( &resp[8] );
            delete ni->foeErrMsg;
            ni->foeErrMsg = 0;

            if( ret > 12 )
            {
               int msgLen = ret-12;
               ni->foeErrMsg = new char[ msgLen+1 ];
               memcpy( ni->foeErrMsg, &resp[12], msgLen );
               ni->foeErrMsg[ msgLen ] = 0;
            }
            return &EtherCatError::FoEerror;
         }

            // ACK or data response.
            // Make sure the packet number matches the expected value.
            // if not, I'll just try sending again
         case 3:
         case 4:
         {
            if( ret < 12 ) return &EtherCatError::FoEformat;

            uint32 num = bytes_to_uint32( &resp[8] );
            if( num != ni->foePacket )
            {
               cml.Debug( "Got %d, expected: %d\n", num, ni->foePacket );
               continue;
            }
            return 0;
         }

         default:
            return &EtherCatError::FoEformat;
      }
   }
   while( (Thread::getTimeMS() - startTime) < to );

   return &ThreadError::Timeout;
}

/**
  Return the detailed error code and message for the most recent FoE error response 
  on this node.

  @param n    Points to the node in question
  @param msg  Buffer where error message should be stored (can be null)
  @param maxMsg  Size of the message buffer
  @return The most recent error code returned by this node.
*/
int32 EtherCAT::FoE_LastErrInfo( Node *n, char *msg, int maxMsg )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return -1;

   if( msg )
   {
      if( ni->foeErrMsg )
         strncpy( msg, ni->foeErrMsg, maxMsg );
      else
         msg[0] = 0;
   }

   return ni->foeErrCode;
}

/**
  Initiate a new file download using the File over EtherCAT (FoE) protocol.
  This function should be used to start a new file download.  On success, the
  EtherCAT::FoE_DnldData function can be used to pass the file data.

  @param n         Points to the node who will receive this download
  @param filename  Name of the file to be sent.  May be null if no file name is to be passed.
  @param password  Optional password value to be sent to the slave device (0 if not used)
  @param to        Timeout to wait for response.
  @return An error pointer on failure, or null on success.
*/
const Error *EtherCAT::FoE_DnldStart( Node *n, const char *filename, uint32 password, Timeout to )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   uint8 *send = ni->syncBuff[ SM_RXMBX ];
   MutexLocker ml( ni->mbxMtx );

   uint16 len = 0;
   if( filename )
      len = strlen(filename);

   // make sure the file name isn't too big for my mailbox
   if( len > ni->syncLen[SM_RXMBX]-12 )
      return &CanOpenError::BadParam;

   // Load the op-code into the FoE header.  Most of the header is initialized by a called function below
   send[6] = 2;

   // Add the password
   int32_to_bytes( password,  &send[8] );

   // file name
   if( filename )
      memcpy( &send[12], filename, len );

   if( ni->foeBuffSize != ni->syncLen[SM_RXMBX]-12 )
   {
      delete ni->foeBuffer;
      ni->foeBuffSize = ni->syncLen[SM_RXMBX]-12;

      cml.Debug( "foeBuffer size %d\n", ni->foeBuffSize );
      ni->foeBuffer = new uint8[ ni->foeBuffSize ];
   }

   ni->foeRemain = 0;
   ni->foePacket = 0;
   ni->foeXferDone = false;
   return FoE_Dnld( n, len+12, to );
}

/**
  Download a block of file data to a node using the File over EtherCAT (FoE) protocol.
  The transfer should have been previously initialized by calling the EtherCAT::FoE_DnldStart
  function.

  @param n     Pointer to the node to download to
  @param len   Length of data to download in bytes
  @param data  Buffer holding the data to download
  @param to    A timeout value.
  @param end   True if this is the last block of data to be sent
  @return An error pointer on failure, or null on success.
*/
const Error *EtherCAT::FoE_DnldData( Node *n, int32 len, uint8 *data, Timeout to, bool end )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   uint8 *buff = ni->syncBuff[ SM_RXMBX ];
   MutexLocker ml( ni->mbxMtx );

   // Find the maximum number of bytes of data that will fit in my mailbox.
   int max = ni->syncLen[ SM_RXMBX ] - 12;

   // Load the op-code into the FoE header.  Most of the header is initialized by a called function below
   buff[6] = 3;

   // The FoE protocol requires that all data packets completely fill the mailbox up to the last one.
   // We may have some bytes remaining from the previous transfer.  Add these to the total number
   // needed to send.
   int R = ni->foeRemain;
   len += R;

   while( len )
   {
      // If this isn't the last packet, and I can't fill my mailbox,
      // then I can't send the data this time.  I'll buffer it for
      // later
      if( (len < max) && !end ) 
         break;

      // Find the number of bytes to send this time
      int ct = (len>max) ? max : len;

      // Copy in any buffered data from a previous call
      memcpy( &buff[12], ni->foeBuffer, R );

      // Add any extra from the current block
      memcpy( &buff[12+R], data, ct-R );

      // Add the packet number
      int32_to_bytes( ++ni->foePacket,  &buff[8] );

      const Error *err = FoE_Dnld( n, ct+12, to );
      if( err ) return err;

      len -= ct;
      data += (ct-R);
      R = 0;
   }

   // Save any extra data for the next call
   memcpy( &ni->foeBuffer[R], data, len-R );
   ni->foeRemain = len;
   return 0;
}

/**
  Initiate a new file upload using the File over EtherCAT (FoE) protocol.
  This function should be used to start a new file upload.  On success, the
  EtherCAT::FoE_UpldData function can be used to read the file data.

  @param n         Points to the node from which the data will be uploaded.
  @param filename  Name of the file to be read.  May be null if no file name is to be passed.
  @param password  Optional password value to be sent to the slave device (0 if not used)
  @param to        Timeout to wait for response.
  @return An error pointer on failure, or null on success.
*/
const Error *EtherCAT::FoE_UpldStart( Node *n, const char *filename, uint32 password, Timeout to )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   uint8 *send = ni->syncBuff[ SM_RXMBX ];
   MutexLocker ml( ni->mbxMtx );

   uint16 len = 0;
   if( filename )
      len = strlen(filename);

   // make sure the file name isn't too big for my mailbox
   if( len > ni->syncLen[SM_RXMBX]-12 )
      return &CanOpenError::BadParam;

   // Load the op-code into the FoE header.  Most of the header is initialized by a called function below
   send[6] = 1;

   // Add the password
   int32_to_bytes( password,  &send[8] );

   // file name
   if( filename )
      memcpy( &send[12], filename, len );

   if( ni->foeBuffSize != ni->syncLen[SM_TXMBX]-12 )
   {
      delete ni->foeBuffer;
      ni->foeBuffSize = ni->syncLen[SM_TXMBX]-12;

      cml.Debug( "foeBuffer size %d\n", ni->foeBuffSize );
      ni->foeBuffer = new uint8[ ni->foeBuffSize ];
   }

   ni->foePacket = 1;
   const Error *err = FoE_Dnld( n, len+12, to );
   if( err ) return err;

   // Save the first block of data returned from the node
   uint8 *resp = ni->syncBuff[ SM_TXMBX ];

   // The data length passed in the mailbox is the number of bytes of FoE data returned, 
   // plus the 6 byte header.
   uint16 retLen = bytes_to_int16( resp ) - 6;

   memcpy( ni->foeBuffer, &resp[12], retLen );
   ni->foeRemain = retLen;

   // If the returned data length isn't the full buffer size, then
   // the transfer is finished.
   ni->foeXferDone = (retLen != ni->foeBuffSize);
   return 0;
}

/**
  Upload a block of file data from a node using the File over EtherCAT (FoE) protocol.
  The transfer should have been previously initialized by calling the EtherCAT::FoE_UpldStart
  function.

  @param n     Pointer to the node to upload from
  @param max   Maximum length of data to upload in bytes
  @param len   On return, holds the actual number of bytes uploaded.
  @param data  Buffer where data will be written
  @param to    A timeout value.
  @return An error pointer on failure, or null on success.
*/
const Error *EtherCAT::FoE_UpldData( Node *n, int32 max, int32 *len, uint8 *data, Timeout to )
{
   EtherCatNodeInfo *ni = GetEcatInfo( n );
   if( !ni ) return &EtherCatError::NodeNotInit;

   if( max < 1 ) return &CanOpenError::BadParam;

   uint8 *send = ni->syncBuff[ SM_RXMBX ];
   MutexLocker ml( ni->mbxMtx );

   // Load the op-code into the FoE header.  Most of the header is initialized by a called function below
   send[6] = 4;

   *len = 0;

   do
   {
      // If the local buffer is empty, fill it
      if( !ni->foeRemain )
      {
         if( ni->foeXferDone )
            return 0;

         // Add the packet number
         int32_to_bytes( ni->foePacket++,  &send[8] );

         const Error *err = FoE_Dnld( n, 12, to );
         if( err ) return err;

         uint8 *resp = ni->syncBuff[ SM_TXMBX ];

         // The data length passed in the mailbox is the number of bytes of FoE data returned, 
         // plus the 6 byte header.
         uint16 retLen = bytes_to_int16( resp ) - 6;

         memcpy( &ni->foeBuffer[ni->foeBuffSize - retLen], &resp[12], retLen );
         ni->foeRemain = retLen;

         // If the returned data length isn't the full buffer size, then
         // the transfer is finished.
         if( retLen != ni->foeBuffSize )
            ni->foeXferDone = true;
      }

      // Copy any data that will fit into the output buffer
      int c = ni->foeRemain;
      if( c > max ) c = max;

      int off = ni->foeBuffSize - ni->foeRemain;
      memcpy( data, &ni->foeBuffer[off], c );
      max -= c;
      ni->foeRemain -= c;
      data += c;
      *len += c;
   } while( max );

   return 0;
}
