/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

// FIXME - need to test timing master support

/** \file

This file holds code for the top level CANopen class.
This class is used for over all control of the CANopen network.

*/

#include "CML.h"

/**
  Frame handling function for time stamp generator.
  This function is called when a new sync frame is received.
*/
CML_NAMESPACE_START()
class TimeStampGenerator: public Receiver
{
private:
   CanOpen *coPtr;
   uint32 timeID;
   int ct;
public:
   const Error *Init( CanOpen &co, uint32 timeID );
   int NewFrame( CanFrame &frame );
};
CML_NAMESPACE_END()


CML_NAMESPACE_USE();

// static CANopen error objects
CML_NEW_ERROR( CanOpenError, ThreadStart,     "Error starting CANopen read thread" );
CML_NEW_ERROR( CanOpenError, BadParam,        "Bad parameter value" );
CML_NEW_ERROR( CanOpenError, SDO_Busy,        "SDO object is not idle" );
CML_NEW_ERROR( CanOpenError, SDO_Timeout,     "Timeout waiting on SDO" );
CML_NEW_ERROR( CanOpenError, SDO_Unknown,     "Unknown SDO error" );
CML_NEW_ERROR( CanOpenError, SDO_BadMuxRcvd,  "Invalid multiplexor received in SDO message" );
CML_NEW_ERROR( CanOpenError, SDO_BadMsgRcvd,  "Invalid format for received SDO message" );
CML_NEW_ERROR( CanOpenError, BadNodeID,       "The specified Node ID was invalid" );
CML_NEW_ERROR( CanOpenError, NotInitialized,  "The referenced object has not been initialized" );
CML_NEW_ERROR( CanOpenError, Initialized,     "The referenced object is already initialized" );
CML_NEW_ERROR( CanOpenError, NotSupported,    "The feature is not supported" );
CML_NEW_ERROR( CanOpenError, MonitorRunning,  "Heartbeat or node guarding already started" );
CML_NEW_ERROR( CanOpenError, IllegalFieldCt,  "Illegal field count returned for the requested object" );
CML_NEW_ERROR( CanOpenError, RcvrNotFound,    "No enabled receiver could be found for that ID" );
CML_NEW_ERROR( CanOpenError, RcvrPresent,     "A CAN receiver using that ID is already enabled" );
CML_NEW_ERROR( CanOpenError, Closed,          "The CANopen port is closed" );

/***************************************************************************/
/**
Default constructor.  Simply initializes some local variables.
*/
/***************************************************************************/
CanOpen::CanOpen( void )
{
   guard.SetCo( this );
   synchProducer = 0;
   errorFrameCt = 0;

   int i;
   for( i=0; i<CML_HASH_SIZE; i++ )
      hash[i] = 0;

   for( i=0; i<128; i++ )
      nodes[i] = 0;

   canRef = 0;
}

/***************************************************************************/
/**
CanOpen Destructor.  This closes the CANopen network.
*/
/***************************************************************************/
CanOpen::~CanOpen( void )
{
   Close();
}

/***************************************************************************/
/**
Close the CANopen network.  This disables all receivers and stops the
thread that listens on the CAN network.
*/
/***************************************************************************/
void CanOpen::Close( void )
{
   ClearHash();
   stop();
   guard.stop();
   ReleaseRef( canRef );
   canRef = 0;
   if( tsGen ) delete tsGen;
   tsGen = 0;
}

/***************************************************************************/
/**
Attach the passed node to this network.
This function is called by the node object when it's initialized.  It connects
the node to the CANopen network object so that messages bound for the node can
be properly delivered.
@param n Pointer to the node to attach to the network.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::AttachNode( Node *n )
{
   int16 id = n->GetNodeID();

   // Check for invalid node ID
   if( id < 1 || id > 127 )
      return &CanOpenError::BadNodeID;

   MutexLocker ml( mtx );
   if( nodes[id] )
      return &NetworkError::NodeIdUsed;

   nodes[id] = n->GrabRef();

   CanOpenNodeInfo *ni = new CanOpenNodeInfo( n );
   SetNodeInfo( n, ni );

   // Initialize the node guarding
   return guard.SetNodeGuard( ni, GUARDTYPE_NONE, 0, 0 );
}

/***************************************************************************/
/**
Detach the passed node from this network.
This function is called by the node object when it's uninitialized.  It removes
the connection between the node and the CANopen network object.
@param n Pointer to the node to detach from the network.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::DetachNode( Node *n )
{
   int16 id = n->GetNodeID();

   // Check for invalid node ID
   if( id < 1 || id > 127 )
      return &CanOpenError::BadNodeID;

   MutexLocker ml( mtx );
   RefObj::ReleaseRef( nodes[id] );
   nodes[id] = 0;
   delete n->nodeInfo;
   SetNodeInfo( n, 0 );

   return 0;
}

/***************************************************************************/
/**
Open the CANopen network.  This function performs the one time initialization 
necessary to communication via the CANopen network.  It should be the first 
function called for the CANopen object.

All configurable settings will be set to their defaults when the CanOpen object
is opened using this method.  For a list of CanOpen object settings and their
default values, please see the CanOpenSettings object.

@param can A reference to the CAN interface object that will be used for
       all low level communication over the network.

@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::Open( CanInterface &can )
{
   CanOpenSettings settings;
   return Open( can, settings );
}

/***************************************************************************/
/**
Open the CANopen network.  This function performs the one time initialization 
necessary to communication via the CANopen network.  It should be the first 
function called for the CANopen object.

This version of the Open function takes a CanOpenSettings object reference
as it's second parameter.  The data members of the settings object may be
used to configure some of the CanOpen object's behavior.

@param ci A reference to the CAN interface object that will be used for
       all low level communication over the network.

@param settings A reference to a CanOpenSettings object.  This object is used
       to customize the behavior of the CanOpen object.

@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::Open( CanInterface &ci, CanOpenSettings &settings )
{
   if( canRef )
      return &CanOpenError::Initialized;

   // Keep a reference to the CAN interface object.
   canRef = ci.GrabRef();

   // Open the low level CAN interface
   const Error *err = ci.Open();

   if( err && (err != &CanError::AlreadyOpen) )
      return err;

   setPriority( settings.readThreadPriority );

   // See if the lower level CAN interface supports timestamps.
   // If not, make sure we aren't trying to be the timing master.
   if( !ci.SupportsTimestamps() )
      settings.useAsTimingReference = false;

   timingMaster = settings.useAsTimingReference;

   // Start a thread that will listen for messages 
   // on the CAN network.
   if( start() )
      return &CanOpenError::ThreadStart;

   if( guard.start() )
      return &CanOpenError::ThreadStart;

   tsGen = 0;
   if( timingMaster )
   {
      tsGen = new TimeStampGenerator();
      if( !tsGen ) return &CanError::Alloc;

      tsGen->Init( *this, settings.timeID );
      err = EnableReceiver( settings.syncID, tsGen );
      if( err ) return err;
   }

   return 0;
}

CanOpenNodeInfo *CanOpen::GetCoInfo( Node *n )
{
   return (CanOpenNodeInfo *)GetNodeInfo(n);
}

/***************************************************************************/
/**
Send a Network Management message to one or more nodes on the network.
@param code The network management function code to be sent.
@param nodeID The ID of the node to send the message to.  Use 0 to broadcast
       the message to all nodes.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::NMT_Msg( int code, int nodeID )
{
   // Check for a valid Node ID
   if( nodeID < 0 || nodeID > 127 )
      return &CanOpenError::BadNodeID;

   CanFrame frame;
   frame.id = 0;
   frame.type = CAN_FRAME_DATA;
   frame.length = 2;
   frame.data[0] = code;
   frame.data[1] = nodeID;

   return Xmit( frame );
}

/***************************************************************************/
/*
Wait for the node state to change to the specified value.
*/
/***************************************************************************/
const Error *CanOpen::WaitNodeState( Node *n, NodeState state, Timeout timeout )
{
   Semaphore sem;
   const Error *err;
   CanOpenNodeInfo *ni = GetCoInfo( n );

   mtx.Lock();
   ni->desired = state;
   ni->semPtr = &sem;
   mtx.Unlock();

   CanFrame frame;
   frame.id = n->GetNodeID() + 0x700;
   frame.type = CAN_FRAME_REMOTE;
   frame.length = 1;

   Timeout remain = timeout;
   Timeout to = 20;
   while(1)
   {
      err = Xmit( frame );
      if( err ) break;

      err = sem.Get( to );
      if( err != &ThreadError::Timeout )
         break;

      remain -= to;
      if( !remain )
      {
         err = &ThreadError::Timeout;
         break;
      }
   }

   mtx.Lock();
   ni->semPtr = 0;
   mtx.Unlock();

   return err;
}

/***************************************************************************/
/**
Send a network management message to start the specified node.  
@param n Pointer to the node to start.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::StartNode( Node *n )
{
   const Error *err = NMT_Msg( 1, n->GetNodeID() );
   if( err ) return err;

   return WaitNodeState( n, NODESTATE_OPERATIONAL, n->sdo.GetTimeout() );
}

/***************************************************************************/
/**
Send a network management message to stop the specified node.  
@param n Pointer to the node to stop.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::StopNode( Node *n )
{
   const Error *err = NMT_Msg( 2, n->GetNodeID() );
   if( err ) return err;

   return WaitNodeState( n, NODESTATE_STOPPED, n->sdo.GetTimeout() );
}

/***************************************************************************/
/**
For CANopen networks, this is the same as CanOpen::StopNode();
@param n Pointer to the node to stop.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::BootModeNode( Node *n )
{
   return StopNode(n);
}

/***************************************************************************/
/**
Send a network management message to put the specified node in pre-operational
state.  
@param n Pointer to the node.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::PreOpNode( Node *n )
{
   const Error *err = NMT_Msg( 128, n->GetNodeID() );
   if( err ) return err;

   return WaitNodeState( n, NODESTATE_PRE_OP, n->sdo.GetTimeout() );
}

/***************************************************************************/
/**
Send a network management message to reset the specified node.  All nodes
are reset if the passed node ID is zero.
@param n A pointer to the node to reset
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::ResetNode( Node *n )
{
   const Error *err;

   // Disable node guarding 
   err = SetNodeGuard( n, GUARDTYPE_NONE );
   if( err ) return err;

   // Reset the node
   err = NMT_Msg( 129, n->GetNodeID() );
   if( err ) return err;

   return WaitNodeState( n, NODESTATE_PRE_OP, n->sdo.GetTimeout() );
}

/***************************************************************************/
/**
Send a network management message to reset the communications of the specified 
node.  All nodes have their communications reset if the passed node ID is zero.
@param n A pointer to the node to reset
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::ResetComm( Node *n )
{
   const Error *err = NMT_Msg( 130, n->GetNodeID() );
   if( err ) return err;

   return WaitNodeState( n, NODESTATE_PRE_OP, n->sdo.GetTimeout() );
}

/***************************************************************************/
/**
Transmit a frame over the CANopen network.
@param frame Refernce to the frame to transmit
@param timeout Max time to wait for the frame to be sent.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::Xmit( CanFrame &frame, Timeout timeout )
{
   RefObjLocker<CanInterface> can( canRef );
   if( !can ) return &CanOpenError::Closed;

   return can->Xmit( frame, timeout );
}

/***************************************************************************/
/**
Transmit a PDO over the CANopen network.  If the PDO is a transmit PDO (i.e. 
the type of PDO that's normally sent from the node and received by the master), 
then a remote request is sent.
@param pdo Pointer to the PDO to be transmitted.
@param timeout Max time to wait for the PDO to be sent.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::XmitPDO( class PDO *pdo, Timeout timeout )
{
   CanFrame frame;

   // Initialize the CAN frame
   frame.id = pdo->GetID();

   if( pdo->IsTxPDO() )
   {
      frame.type = CAN_FRAME_REMOTE;
      frame.length = (pdo->GetBitCt()+7)>>3;
   }
   else
   {
      RPDO *rpdo = (RPDO*)pdo;
      frame.type = CAN_FRAME_DATA;
      frame.length = rpdo->LoadData( frame.data, 8 );
   }

   // Transmit the frame
   return Xmit( frame, timeout );
}

/***************************************************************************/
/**
Transmit an SDO message over the CANopen network and wait for a response.  
The SDO should have alrady been formatted into the passed array.  This function 
assigns the standard CAN message ID and transmits the resulting CAN frame.

@param n Pointer to the node to whom the SDO is addressed.
@param buff Buffer containing the formatted SDO data
@param len  Length of the SDO data in bytes.  Should be 8 bytes for standard CANopen SDOs.
@param ret  Pointer to a location where the number of received bytes will be stored on success.
@param timeout Max time to wait for the PDO to be sent.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::XmitSDO( Node *n, uint8 *buff, uint16 len, uint16 *ret, Timeout timeout )
{
   CanOpenNodeInfo *ni = GetCoInfo(n);
   CanFrame frame;
   Semaphore sem;

   // SDOs must be 8 bytes long for CANopen
   if( len != 8 )
      return &CanOpenError::BadParam;

   *ret = 8;

   frame.id = 0x600 + n->GetNodeID();
   frame.type = CAN_FRAME_DATA;
   frame.length = 8;
   for( int i=0; i<8; i++ )
      frame.data[i] = buff[i];

   if( timeout )
   {
      mtx.Lock();
      ni->sdoBuff = buff;
      ni->sdoSemPtr = &sem;
      mtx.Unlock();
   }

   const Error *err = Xmit( frame, 1000 );
   if( err ) goto done;

   if( !timeout ) return 0;

   err = sem.Get( timeout );
   if( err == &ThreadError::Timeout )
      err = &CanOpenError::SDO_Timeout;

done:
   mtx.Lock();
   ni->sdoBuff = 0;
   ni->sdoSemPtr = 0;
   mtx.Unlock();
   return err;
}

/***************************************************************************/
/**
CAN network read thread.  This function defines the thread that will be
used to read the CAN network and pass received frames to the various 
CANopen network reader objects.
*/
/***************************************************************************/
void CanOpen::run( void )
{
   const Error *err;
   CanFrame frame;

   while( canRef )
   {
      {
         RefObjLocker<CanInterface> can( canRef );

         // If the port is closed, return.  This will stop the thread.
         if( !can )
         {
            cml.Error( "CAN interface deleted while CANopen read thread running, aborting CANopen thread\n" );
            return;
         }

         err = can->Recv( frame, 200 );
      }

      // Ignore timeouts
      if( (err==&ThreadError::Timeout) || (err==&CanError::Timeout)  )
         continue;

      if( err )
      {
         cml.Error( "Error reading from CAN interface: %s\n", err->toString() );
         sleep( 5 );
         continue;
      }

      if( frame.type == CAN_FRAME_ERROR )
      {
         errorFrameCt++;
         continue;
      }

      uint32 rcvrRef = LookupReceiver( frame.id );
      if( rcvrRef )
      {
         RefObjLocker<Receiver> r( rcvrRef );
         if( r ) r->NewFrame(frame);
      }

      // Try doing some default handling for some standard messages
      if( frame.type == CAN_FRAME_DATA )
      {
         uint32 type = frame.id & 0xFFFFFF80;

         RefObjLocker<Node> n( nodes[ frame.id & 0x7F ] );
         if( !n ) continue;

         CanOpenNodeInfo *ni = GetCoInfo(n);

         switch( type )
         {
            // Emergency object
            case 0x00000080:
               n->HandleEmergency( frame );
               break;

               // Node guarding
            case 0x00000700:
            {
               MutexLocker ml( mtx );
               guard.HandleNMT( frame, ni );
               break;
            }

               // SDO response.
            case 0x00000580:
            {
               MutexLocker ml( mtx );
               if( ni->sdoSemPtr )
               {

                  int i;
                  for( i=0; i<8; i++ )
                     ni->sdoBuff[i] = frame.data[i];
                  ni->sdoSemPtr->Put();
                  ni->sdoSemPtr = 0;
               }
               break;
            }
         }
      }
   }
}

/***************************************************************************/
/**
Default constructor for CanOpenSettings object.
This constructor simply sets all the settings to their default values.
*/
/***************************************************************************/
CanOpenSettings::CanOpenSettings( void )
{
   readThreadPriority = 9;
   useAsTimingReference = false;
   syncID = 0x080;
   timeID = 0x180;
}

/***************************************************************************/
/**
Configure the node guarding protocol for a CANopen node.

@param n       The node to configure guarding on.
@param type    The type of node guarding to configure.
@param to      A timeout (milliseconds) to use for this node guarding protocol.
               If not specified, this parameter defaults to 50 milliseconds.
@param life    A life time factor for use with the node guarding protocol (default 3).
               Only the node guarding protocol uses this parameter, it may be 
               omitted for other node guarding types.
@return A pointer to an error object, or NULL on success.
 */
/***************************************************************************/
const Error *CanOpen::SetNodeGuard( Node *n, GuardProtocol type, Timeout to, uint8 life )
{
   int32 timeout = (int32)to;
   const Error *err;

   switch( type )
   {
      case GUARDTYPE_NONE:
         // Clear both heartbeat and node guard times.
         err = n->sdo.Dnld16( 0x1017, 0, (uint16)0 );
         if( err ) return err;
         err = n->sdo.Dnld16( 0x100C, 0, (uint16)0 );
         if( err ) return err;
         break;

      case GUARDTYPE_HEARTBEAT:
         // If the timeout is sent as zero, return an error.
         if( !timeout )
            return &CanOpenError::BadParam;

         // Set the node's heartbeat time.
         if( timeout > 65535 ) timeout = 65535;
         err = n->sdo.Dnld16( 0x1017, 0, (uint16)timeout );
         if( err ) return err;
         break;

      case GUARDTYPE_NODEGUARD:
         // If guard time is zero, return an error.
         // Note that the lifeFactor can be zero, this disables the life
         // guarding inside the node.
         if( !timeout )
            return &CanOpenError::BadParam;

         // Program the life time factor
         err = n->sdo.Dnld8( 0x100D, 0, life );
         if( err ) return err;

         // Program the guard time
         if( timeout > 65535 ) timeout = 65535;
         err = n->sdo.Dnld16( 0x100C, 0, (uint16)timeout );
         if( err ) return err;

         // Try to set the heartbeat time to zero.
         // This may fail (since the heartbeat may not be
         // supported).
         n->sdo.Dnld16( 0x1017, 0, (uint16)0 );
         break;

      default:
         return &CanOpenError::BadParam;
   }

   return guard.SetNodeGuard( GetCoInfo(n), type, timeout, life );
}

/***************************************************************************/
/**
This method handles network management messages received from nodes on the 
network.  These messages are part of the node guarding / heartbeat mechanism
of CANopen.
*/
/***************************************************************************/
void CanOpen::NodeGuardThread::HandleNMT( CanFrame &frame, CanOpenNodeInfo *ni )
{
   // Check for a valid toggle bit.  The toggle bit is only
   // used when running in node guarding mode. 
   int toggleErr = ni->guardToggle ^ (frame.data[0] & 0x80);

   // If the guardToggle variable is -1, then ignore toggle
   // errors.  This is used in the modes where the toggle bit
   // is not defined (heartbeat and no guarding).  It's also
   // used for the first node guarding message since I can't be
   // sure what the node's state is when I start and therefore
   // don't know if the toggle bit should be set.
   if( ni->guardToggle < 0 )
      toggleErr = 0;

   // If I'm using the node guarding protocol, update my
   // expected toggle bit value
   if( ni->guardType == GUARDTYPE_NODEGUARD )
      ni->guardToggle = (~frame.data[0]) & 0x80;

   if( toggleErr )
   {
      cml.Warn( "Node %d guard toggle error\n", ni->me->GetNodeID() );
      return;
   }

   // Update the node state info
   NodeState newState = NODESTATE_INVALID;
   switch( frame.data[0] & 0x7f )
   {
      case 0:   newState = NODESTATE_PRE_OP;      break;
      case 4:   newState = NODESTATE_STOPPED;     break;
      case 5:   newState = NODESTATE_OPERATIONAL; break;
      case 127: newState = NODESTATE_PRE_OP;      break;
   }

   if( newState != NODESTATE_INVALID )
   {
      ni->me->SetState( newState );

      // Note the CanOpen thread's mutex is held while this function is called
      if( (newState==ni->desired) && ni->semPtr )
         ni->semPtr->Put();
   }

   MutexLocker ml( mtx );

   // Reset the life counter (used with node guarding)
   ni->lifeCounter = 0;

   // Re-insert this node in my timeout list
   // if running the heartbeat protocol
   if( ni->guardType == GUARDTYPE_HEARTBEAT )
      AddNode( ni, getTimeMS() + ni->guardTimeout );

   sem.Put();
   return;
}

CanOpen::NodeGuardThread::NodeGuardThread( void ): CanOpenNodeInfo(0){}

CanOpenNodeInfo::CanOpenNodeInfo( Node *node )
{
   me = node;
   guardToggle = -1;
   guardTimeout = 10000;
   lifeTime = 1;
   lifeCounter = 0;
   guardType = GUARDTYPE_NONE;
   desired = NODESTATE_INVALID;
   semPtr = 0;
   sdoSemPtr = 0;
   sdoBuff = 0;
   next = prev = this;
}

CanOpenNodeInfo::~CanOpenNodeInfo( void )
{
}

void CanOpenNodeInfo::Unlink( void )
{
   prev->next = next;
   next->prev = prev;
   next = prev = this;
}

// Add the passed node to my sorted list.  The nodes are sorted by
// time when they need to next be accessed.
// Note that my mutex is expected to be held when this is called.
void CanOpen::NodeGuardThread::AddNode( CanOpenNodeInfo *ni, uint32 time )
{
   // Unlink this node
   ni->Unlink();

   // Don't add nodes with no guarding enabled
   if( ni->guardType == GUARDTYPE_NONE )
      return;

   // Set the head's event time to ensure that I don't 
   // loop forever below.
   eventTime = time+1;

   // Find the correct place to insert this node
   CanOpenNodeInfo *ptr = next; 
   while( (int32)(ptr->eventTime-time) <= 0 )
      ptr=ptr->next;

   // Inser the node
   ni->prev = ptr->prev;
   ni->next = ptr;
   ni->prev->next = ni;
   ni->next->prev = ni;
   ni->eventTime = time;

   return;
}

const Error *CanOpen::NodeGuardThread::SetNodeGuard( CanOpenNodeInfo *ni, GuardProtocol type, int32 timeout, uint8 life )
{
   MutexLocker ml( mtx );
   ni->Unlink();
   ni->guardType = type;
   ni->guardToggle = -1;
   ni->guardTimeout = timeout;
   ni->lifeTime = life;
   ni->lifeCounter = 0;
   AddNode( ni, getTimeMS()+timeout );
   sem.Put();
   return 0;
}

void CanOpen::NodeGuardThread::run( void )
{
   int32 timeout = -1;

   while( 1 )
   {
      sem.Get( (Timeout)timeout );

      while( 1 )
      {
         uint32 now = getTimeMS();

         MutexLocker ml( mtx );

         // Set the event time of the root of the list to 1 second
         // from now.  If the list is empty, this will be my timeout
         eventTime = now+1000;

         // Break when I get to an entry in the list that hasn't expired
         timeout = next->eventTime - now;
         if( timeout > 0 )
            break;

         CanOpenNodeInfo *ni = (CanOpenNodeInfo *)next;

         switch( ni->guardType )
         {
            // If I timeout when using the heartbeat protocol, it means
            // that the node didn't respond in time.
            case GUARDTYPE_HEARTBEAT:
               ni->me->SetState( NODESTATE_GUARDERR );
               break;

               // I always expect to timeout with the node guarding protocol.
               // This means that I need to send a new guard message.
            case GUARDTYPE_NODEGUARD:
               {
                  CanFrame frame;

                  frame.id = ni->me->GetNodeID() + 0x700;
                  frame.type = CAN_FRAME_REMOTE;
                  frame.length = 1;

                  co->Xmit( frame );

                  // See if my life time has expired.  If so, flag a 
                  // node guarding error
                  ni->lifeCounter++;
                  if( ni->lifeCounter > ni->lifeTime )
                  {
                     ni->me->SetState( NODESTATE_GUARDERR );
                     ni->lifeCounter = 0;
                  }

                  break;
               }

               // This really shouldn't ever happen.  If no node guarding is
               // enabled then the node shouldn't be on my queue.
            case GUARDTYPE_NONE:
            default:
               next->Unlink();
               continue;
         }

         // Re-insert this node into my list at the appropriate time
         AddNode( next, now + ni->guardTimeout );
      }
   }
}

struct CoTpdoInfo: public Receiver
{
   uint32 ref;

   CoTpdoInfo( CanOpen &co, TPDO *pdo )
   {
      ref = pdo->GrabRef();
      SetRefName("CoTpdoInfo");
      setAutoDelete( true );
   }
   ~CoTpdoInfo()
   {
      RefObj::ReleaseRef( ref );
   }

   int NewFrame( CanFrame &frame )
   {
      if( frame.type != CAN_FRAME_DATA )
         return 0;

      RefObjLocker<TPDO> pdo( ref );
      if( pdo ) pdo->ProcessData( frame.data, frame.length, frame.timestamp );

      return 0;
   }
};

const Error *CanOpen::PdoEnable( Node *node, uint16 slot, PDO *pdo )
{
   const Error *err;

   // Make sure the slot is reasonable.
   if( slot > 511 )
      return &CanOpenError::BadParam;

   // Find the correct location in the object dictionary
   uint16 n = pdo->IsTxPDO() ? 0x1800 : 0x1400;
   n += slot;

   // Make sure PDO is currently disabled
   err = PdoDisable( node, slot, pdo );
   if( err ) return err;

   // Set the PDO type if necessary
   uint8 oldType;
   err = node->sdo.Upld8( n, 2, oldType );
   if( err ) return err;

   uint8 newType = pdo->GetType();
   if( oldType != newType )
   {
      err = node->sdo.Dnld8( n, 2, newType );
      if( err ) return err;
   }

   // Find the ID number associated with this PDO.
   uint32 id = pdo->GetID();

   // Use the default for this slot number if the ID hasn't been set
   if( id == 0xFFFFFFFF )
   {
      // The default ID values are defined by the CANopen standard for 
      // PDO slots 0-3.  For other slots I use extended CAN message IDs.
      id = slot * 0x100 + node->GetNodeID();
      id += pdo->IsTxPDO() ? 0x180 : 0x200;
      if( slot > 3 ) id += 0x20000000;
      pdo->SetID( id );
   }

   // Set bit 30 of the ID parameter if this PDO doesn't support 
   // remote requests (really only valid for transmit PDOs).
   if( !pdo->GetRtrOk() ) id |= 0x40000000;

   // Set the new ID value
   err = node->sdo.Dnld32( n, 1, id );
   if( err ) return err;

   // If this is a transmit PDO, allocate a receiver to handle
   // messages received from it over the network
   if( pdo->IsTxPDO() )
   {
      CoTpdoInfo *pdoInfo = new CoTpdoInfo( *this, (TPDO*)pdo );
      if( !pdoInfo ) return &CanError::Alloc;
      err = EnableReceiver( pdo->GetID(), pdoInfo );
      if( err ) return err;
   }

   return 0;
}

const Error *CanOpen::PdoDisable( Node *node, uint16 slot, PDO *pdo )
{
   return PdoDisable( node, slot, pdo->IsTxPDO() );
}

const Error *CanOpen::PdoDisable( Node *node, uint16 slot, bool isTxPDO )
{
   // Make sure the slot is reasonable.
   if( slot > 511 )
      return &CanOpenError::BadParam;

   // Find the correct location in the object dictionary
   uint16 n = isTxPDO ? 0x1800 : 0x1400;
   n += slot;

   // Upload the PDO's ID
   uint32 id;
   const Error *err = node->sdo.Upld32( n, 1, id );
   if( err ) return err;

   // If it's enabled, disable it
   if( !(id & 0x80000000) )
      err = node->sdo.Dnld32( n, 1, (uint32)(id|0x80000000) );

   if( LookupReceiver( id ) )
      err = DisableReceiver( id );

   return err;
}

/***************************************************************************/
/**
Default constructor for a network receiver object.
*/
/***************************************************************************/
Receiver::Receiver( void )
{
}

/***************************************************************************/
/**
Destructor for network receiver objects.  This destructor ensures that the
receiver is disabled before it is destroyed.
*/
/***************************************************************************/
Receiver::~Receiver()
{
   KillRef();
}

/***************************************************************************/
/**
Process a new received CAN bus frame.  This virtual function is called by
the CANopen read thread every time a CAN frame is received over the network
with an ID matching the receivers ID if the receiver is enabled.  

Note that this function is called from the CANopen receive thread.  No other
receive frames will be processed until this function returns.

Also note that the map object used to associate message IDs with receive objects
is locked when this function is called.  The locking is required to prevent a 
race condition that could occur when a receive object is disabled and it's 
memory is deallocated.  Since the map is locked, it's illegal to Enable() or
Disable() any receive object from within this function.

@param frame The CAN frame to be processed.  Note that the memory holding the
       frame structure may be reused after the call returns.  If the frame 
       contents are to be used after the return the a copy of the frame should
       be made. 

@return non-zero if the frame was handled, zero if the frame type was unknown.
*/
/***************************************************************************/
int Receiver::NewFrame( CanFrame &frame )
{
   return 0;
}

CML_NAMESPACE_START()
class coHashEntry
{
public:
   coHashEntry *next;
   uint32 canMsgID;
   uint32 rcvrRef;

   coHashEntry( uint32 msgID, Receiver *rcvr )
   {
      canMsgID = msgID;
      next = 0;

      rcvrRef = rcvr->GrabRef();
   }

   ~coHashEntry()
   {
      RefObj::ReleaseRef( rcvrRef );
   }
};
CML_NAMESPACE_END()


/***************************************************************************/
/**
Search a hash table of receiver objects associated with this network.
The CanOpen object maintains a hash of all enabled receivers on the 
network.  Each time a new CAN frame is received, the hash is searched for
a receiver matching the frame's ID.  This function handles the details of
the search.

This function is also used when adding receivers to the tree.  To aid this
use, it actually returns a pointer to a pointer to the receiver, or to the
place where the new receiver would be added.

Note, it's assumed that the hash mutex will be locked when this function 
is called.

@param id The CAN message ID of the receiver to be found.
@return A pointer to a pointer to the hash entry, or the location where the
        entry should be added if there is none with a matching ID.
*/
/***************************************************************************/
coHashEntry **CanOpen::searchHash( uint32 id )
{
   int i = id % CML_HASH_SIZE;

   coHashEntry **pp = &hash[i];

   while( 1 )
   {
      if( !*pp )
         return pp;

      uint32 x = (*pp)->canMsgID;

      if( id == x )
         return pp;

      pp = &( (*pp)->next);
   }
}

/***************************************************************************/
/**
Enable reception handling of the message identified by this Receiver
object.  The receiver is enabled by adding it to a binary tree of 
receiver objects maintained by the CanOpen object.

@param msgID The CAN message ID to assocaite with this receiver.
@param rcvr Pointer to the receiver to add
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::EnableReceiver( uint32 msgID, Receiver *rcvr )
{
   MutexLocker ml( hashMtx );

   coHashEntry **pp = searchHash( msgID );

   if( *pp )
      return &CanOpenError::RcvrPresent;

   *pp = new coHashEntry( msgID, rcvr );
   return 0;
}

/***************************************************************************/
/**
Disable reception handling of a particular CANopen message type.

@param canMsgID  The CAN message ID.
@return A pointer to an error object, or NULL on success.
*/
/***************************************************************************/
const Error *CanOpen::DisableReceiver( uint32 canMsgID )
{
   MutexLocker ml( hashMtx );

   coHashEntry **pp = searchHash( canMsgID );

   if( !*pp )
      return &CanOpenError::RcvrNotFound;

   coHashEntry *entry = *pp;
   *pp = entry->next;
   delete entry;
   return 0;
}

/**
 * Search the hash table for a receiver associated with this message ID.
 *
 * @param canMsgID The CAN message ID to search for.
 * @return A reference to the receiver, or 0 if no receiver is found.
 */
uint32 CanOpen::LookupReceiver( uint32 canMsgID )
{
   MutexLocker ml( hashMtx );
   coHashEntry *entry = *searchHash( canMsgID );
   if( !entry ) return 0;
   return entry->rcvrRef;
}

void CanOpen::ClearHash( void )
{
   MutexLocker ml( hashMtx );

   for( int i=0; i<CML_HASH_SIZE; i++ )
   {
      coHashEntry *entry = hash[i];
      while( entry )
      {
         coHashEntry *next = entry->next;
         delete entry;
         entry = next;
      }
      hash[i] = 0;
   }
}


const Error *TimeStampGenerator::Init( CanOpen &co, uint32 timeID )
{
   this->timeID = timeID;
   this->coPtr = &co;
   ct = 0;
   return 0;
}

int TimeStampGenerator::NewFrame( CanFrame &frame )
{
   if( ++ct < 10 )
      return 1;
   ct = 0;

   uint32 ts = frame.timestamp;

   CanFrame msg;
   msg.type   = CAN_FRAME_DATA;
   msg.id     = timeID;
   msg.length = 4;
   msg.data[0] = ByteCast(ts );
   msg.data[1] = ByteCast(ts>> 8);
   msg.data[2] = ByteCast(ts>>16);
   msg.data[3] = ByteCast(ts>>24);

   coPtr->Xmit( msg );
   return 1;
}
