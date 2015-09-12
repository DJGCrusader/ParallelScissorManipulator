/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/** \file
This file holds code to implement the CANopen node related objects.
*/

#include "CML.h"

CML_NAMESPACE_USE();

/**************************************************
* Node Error objects
**************************************************/
CML_NEW_ERROR( NodeError, GuardTimeout, "Node guarding timeout" );
CML_NEW_ERROR( NodeError, NetworkUnavailable, "The network this node is connected to has been deleted" );

/**************************************************
* Bits used with the node guarding event map
**************************************************/
#define GUARD_EVENT_MSG_RVCD     0x00000001
#define GUARD_EVENT_CHANGE       0x00000002
#define GUARD_EVENT_DSRDSTATE    0x00000004

/***************************************************************************/
/**
Default CANopen node object constructor.  This constructor simple marks the
object as uninitialized.  The Init() function must be called before this
object can be used.
*/
/***************************************************************************/
Node::Node(): RefObj( "Node" )
{
   netRef = 0;
   nodeInfo = 0;
   nodeID = 0;
}

/***************************************************************************/
/**
Initialize the Node object.
@param net The network object that this node is associated with.
@param nodeID The node's ID.  
*/
/***************************************************************************/
Node::Node( Network &net, int16 nodeID ): RefObj( "Node" )
{
   netRef = 0;
   nodeInfo = 0;
   Init( net, nodeID );
}

/***************************************************************************/
/**
CANopen node destructor.
*/
/***************************************************************************/
Node::~Node()
{
   if( IsInitialized() )
   {
      cml.Debug( "Node: %d destroyed\n", nodeID );
      KillRef();
      UnInit();
   }
   if( nodeInfo ) delete nodeInfo;
}

/***************************************************************************/
/**
Initialize the CANopen Node object.  Note that a CANopen node object must
be initialized once and only once.  This function should be used to initialize
the object if it was created using the default constructor.

@param network The network object that this node is associated with.
@param nodeID  The node's ID.  A value that identifies this node on the network.
@return A pointer to an error object, or NULL on success
*/
/***************************************************************************/
const Error *Node::Init( Network &network, int16 nodeID )
{
   // Un-initialize first if necessary
   const Error *err = UnInit();
   if( err ) return err;

   // Init some local parameters
   this->nodeID = nodeID;
   state = NODESTATE_UNKNOWN;

   // Grab a reference to the passed network object
   netRef = network.GrabRef();

   // Init my SDO
   err = sdo.Init( this );

   // Attach this node to the network
   if( !err )
      err = network.AttachNode( this );

   // If I fail to attach to the network, the 
   // discard my network reference
   if( err )
   {
      RefObj::ReleaseRef( netRef );
      netRef = 0;
   }

   return err;
}

/***************************************************************************/
/**
Un-initialize the Node object.  This puts the object back to it's default
state.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *Node::UnInit( void )
{
   // Detach this node from the network
   if( netRef ) 
   {
      Network *net = (Network*)RefObj::LockRef( netRef );
      if( net )
      {  
         net->DetachNode( this );
         net->UnlockRef();
      }
      RefObj::ReleaseRef( netRef );
      netRef = 0;
   }

   return 0;
}

/***************************************************************************/
/**
Associate the passed PDO object with this node.  The PDO
will be setup as this node's nth PDO.

@param slot Which PDO slot to assign this PDO to.
@param pdo The PDO object.
@param enable If true, the PDO will be enabled after being setup (default).
              If false, the PDO will be setup but not enabled.
@return A pointer to an error object, or NULL on success
*/
/***************************************************************************/
const Error *Node::PdoSet( uint16 slot, PDO &pdo, bool enable )
{
   // Make sure the slot is reasonable.
   if( slot > 511 )
      return &CanOpenError::BadParam;

   // Disable any PDO currently in this slot
   const Error *err = PdoDisable( slot, pdo );

   // Get the PDO variable mapping info
   uint32 codes[PDO_MAP_LEN];
   byte ct = pdo.GetMapCodes( codes );

   uint16 base = pdo.IsTxPDO() ? 0x1A00 : 0x1600;
   base += slot;

   // Compare the new mapping info to the info already
   // in the PDO.  If it's different, then I'll update it.
   byte pdoCt;
   err = sdo.Upld8( base, 0, pdoCt );
   if( err ) return err;

   bool done = false;

   if( pdoCt == ct )
   {
      uint32 var;
      byte i;
      for( i=0; i<ct; i++ )
      {
         err = sdo.Upld32( base, i+1, var );
         if( err ) return err;

         if( var != codes[i] )
            break;
      }
      if( i == ct )
         done = true;
   }

   if( !done )
   {
      // Clear out any old mapping
      err = sdo.Dnld8( base, 0, (byte)0 );
      if( err ) return err;

      // Download the new mapping
      for( byte i=0; i<ct; i++ )
      {
         err = sdo.Dnld32( base, i+1, codes[i] );
         if( err ) return err;
      }

      // Active the new mapping
      err = sdo.Dnld8( base, 0, ct );
      if( err ) return err;
   }

   // Enable the PDO if so requested
   if( enable )
      return PdoEnable( slot, pdo );

   return 0;
}

/***************************************************************************/
/**
Return a value that identifies the type of network the node is currently
attached to.
@return A network type value, or NET_TYPE_INVALID if the node isn't attached
        to any network.
*/
/***************************************************************************/
NetworkType Node::GetNetworkType( void )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return NET_TYPE_INVALID;

   return net->GetNetworkType();
}

/***************************************************************************/
/**
Return a reference ID to the network that this node is attached to.
@return The reference ID or 0 if the node isn't attached to any network.
*/
/***************************************************************************/
uint32 Node::GetNetworkRef( void )
{
   return netRef;
}

/***************************************************************************/
/**
Enable the passed PDO object
@param n The slot number of the PDO
@param pdo The PDO mapped to that slot
@return An error object
*/
/***************************************************************************/
const Error *Node::PdoEnable( uint16 n, PDO &pdo )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->PdoEnable( this, n, &pdo );
}

/***************************************************************************/
/**
Disable the passed PDO object
@param n The slot number of the PDO
@param pdo The PDO mapped to that slot
@return An error object
*/
/***************************************************************************/
const Error *Node::PdoDisable( uint16 n, PDO &pdo )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->PdoDisable( this, n, &pdo );
}

/***************************************************************************/
/**
Disable the specified receive PDO.
@param n The slot number of the PDO
@return An error object
*/
/***************************************************************************/
const Error *Node::RpdoDisable( uint16 n )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->PdoDisable( this, n, false );
}

/***************************************************************************/
/**
Disable the specified transmit PDO.
@param n The slot number of the PDO
@return An error object
*/
/***************************************************************************/
const Error *Node::TpdoDisable( uint16 n )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->PdoDisable( this, n, true );
}

/***************************************************************************/
/**
Get the error history array (CANopen object 0x1003).

@param ct    When the function is first called, this variable holds the maximum number
             of errors that can be stored in the err array (i.e. the length of the array).
             On return, the actual number of errors uploaded will be stored here.
@param array An array of 32-bit integers that will be used to return the list of errors.
@return A pointer to an error object, or NULL on success
*/
/***************************************************************************/
const Error *Node::GetErrorHistory( uint16 &ct, uint32 *array )
{
   uint32 i;

   // Upload the first element in the error array.
   // This holds the actual number of errors in it's 
   // lowest byte.
   const Error *err = sdo.Upld32( 0x1003, 0, i );
   if( err ) return err;

   if( i > 254 ) i = 254;

   // Limit the number of errors to download to ct
   if( i < (uint32)ct ) ct = i;

   for( i=1; i<=(uint32)ct; i++ )
   {
      err = sdo.Upld32( 0x1003, i, array[i-1] );
      if( err ) return err;
   }

   return 0;
}

/***************************************************************************/
/**
Get the CANopen identity object for this node (object dictionary entry 0x1018).
Note that only the VendorID field is mandatory.  Any unsupported fields will
be returned as zero.

@param id The identity object to be filled in by this call
@return A pointer to an error object, or NULL on success
*/
/***************************************************************************/
const Error *Node::GetIdentity( NodeIdentity &id )
{
   // Find the number of supported fields
   int8 ct;
   const Error *err = sdo.Upld8( 0x1018, 0, ct );
   if( err ) return err;

   if( ct < 1 )
      return &CanOpenError::IllegalFieldCt;

   id.productCode = 0;
   id.revision    = 0;
   id.serial      = 0;

   err = sdo.Upld32( 0x1018, 1, id.vendorID );
   if( !err && ct > 1 ) err = sdo.Upld32( 0x1018, 2, id.productCode );
   if( !err && ct > 2 ) err = sdo.Upld32( 0x1018, 3, id.revision    );
   if( !err && ct > 3 ) err = sdo.Upld32( 0x1018, 4, id.serial      );

   return err;
}

/***************************************************************************/
/**
Start producing SYNC messages on this node.
@return An error object.
*/
/***************************************************************************/
const Error *Node::SynchStart( void )
{
   // Ignore this for EtherCAT networks
   if( GetNetworkType() != NET_TYPE_CANOPEN )
      return 0;

   uint32 id;
   const Error *err = GetSynchId( id );
   if( !err )
      err = SetSynchId( id | 0x40000000 );
   if( err ) return err;

   RefObjLocker<CanOpen> co( GetNetworkRef() );
   if( !co ) return &NodeError::NetworkUnavailable;

   co->SetSynchProducer( nodeID );
   return 0;
}

/***************************************************************************/
/**
Stop producing SYNC messages on this node.
@return An error object.
*/
/***************************************************************************/
const Error *Node::SynchStop( void )
{
   // Ignore this for EtherCAT networks
   if( GetNetworkType() != NET_TYPE_CANOPEN )
      return 0;

   uint32 id;
   const Error *err = GetSynchId( id );
   if( !err && (id&0x40000000) )
      err = SetSynchId( id & 0x3FFFFFFF );

   RefObjLocker<CanOpen> co( GetNetworkRef() );
   if( !co ) return &NodeError::NetworkUnavailable;

   if( !err && (co->GetSynchProducer() == nodeID) )
      co->SetSynchProducer(0);

   return err;
}

/***************************************************************************/
/**
Reset this node.
@return An error object
*/
/***************************************************************************/
const Error *Node::ResetNode( void )
{
   cml.Debug( "ResetNode %d.\n", GetNodeID() );

   // Send the node a reset message
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->ResetNode( this );
}

/***************************************************************************/
/**
Reset this node's communications.
@return An error object or null on success.
*/
/***************************************************************************/
const Error *Node::ResetComm( void )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->ResetComm( this );
}

/***************************************************************************/
/**
Start this node.
@return An error object or null on success.
*/
/***************************************************************************/
const Error *Node::StartNode( void )
{ 
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->StartNode( this );
}

/***************************************************************************/
/**
Stop this node
@return An error object or null on success.
*/
/***************************************************************************/
const Error *Node::StopNode( void )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->StopNode( this );
}

/***************************************************************************/
/**
Put this node in pre-operational state
@return An error object or null on success.
*/
/***************************************************************************/
const Error *Node::PreOpNode( void )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->PreOpNode( this );
}

/***************************************************************************/
/**
Disable node guarding & heartbeat monitoring.

@return A pointer to an error object, or NULL on success
*/
/***************************************************************************/
const Error *Node::StopGuarding( void )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->SetNodeGuard( this, GUARDTYPE_NONE );
}

/***************************************************************************/
/**
Enable heartbeat messages from this node, and start a thread to monitor
them.
@param period The producer timeout value (milliseconds).  The node will
       be configured to produce a heartbeat message at this interval.

@param timeout The additional number of milliseconds that the monitor
       thread will wait before indicating an error.  Thus, the consumer
       heartbeat interval will be (period + timeout).

@return A pointer to an error object, or NULL on success
*/
/***************************************************************************/
const Error *Node::StartHeartbeat( uint16 period, uint16 timeout )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->SetNodeGuard( this, GUARDTYPE_HEARTBEAT, (Timeout)period + (Timeout)timeout );
}

/***************************************************************************/
/**
Enable node guarding on this node.

When node guarding is enabled, a new thread is created which will send a 
remote request to this node every (guardTime) milliseconds.  The node must
respond to this message within the guard time.  If the node does not respond
then the thread will notify the node of a state change.

@param guardTime The period in milliseconds of the guard messages sent
       to the node.  It can range from 1 to 65535.

@param lifeFactor A multiplier used by the node to determine how long to 
       wait for a node guarding message from the host before indicating
       a local error.  The nodes timeout (life time) is guardTime * lifeFactor.
       This parameter must be between 0 and 255.  If it's zero, then life
       guarding on the node is disabled.

@return A pointer to an error object, or NULL on success
*/
/***************************************************************************/
const Error *Node::StartNodeGuard( uint16 guardTime, byte lifeFactor )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->SetNodeGuard( this, GUARDTYPE_NODEGUARD, guardTime, lifeFactor );
}

/***************************************************************************/
/*
Set the node state.  This should only be called by the controlling network class.
@param newState New node state to set.
*/
/***************************************************************************/
void Node::SetState( NodeState newState )
{
   NodeState old;

   {
      MutexLocker ml( mtx );
      old = state;
      state = newState;
   }

   if( old != newState )
      HandleStateChange( old, newState );
}

/// Return the maximum number of bytes that can be sent in an SDO message.
/// For CANopen this is always 8 (the max size of a CAN frame).  For EtherCAT
/// it's the size of the mailbox buffer, and is node specific
/// @return The maximum number of bytes in an SDO transmit message, or 0 on error
int32 Node::maxSdoToNode( void )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return 0;
   return net->maxSdoToNode( this );
}

/// Return the maximum number of bytes that can be received in an SDO message.
/// For CANopen this is always 8 (the max size of a CAN frame).  For EtherCAT
/// it's the size of the mailbox buffer, and is node specific
/// @return The maximum number of bytes in an SDO receive message, or 0 on error
int32 Node::maxSdoFromNode( void )
{
   RefObjLocker<Network> net( GetNetworkRef() );
   if( !net ) return 0;
   return net->maxSdoFromNode( this );
}

