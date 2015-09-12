/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/** \file
This file contains code that implements PDO objects used by the
Copley Controls amplifier object.
*/

#include "CML.h"

#define MAX_AXIS    2

/****************************************************************
* CANopen drive status bits (CANopen 402 Object 0x6041)
*****************************************************************/
#define DRIVESTAT_RTSO                      0x0001   // Ready to switch on
#define DRIVESTAT_ON                        0x0002   // Switched on
#define DRIVESTAT_ENA                       0x0004   // Operation enabled
#define DRIVESTAT_FAULT                     0x0008   // A fault has occurred on the drive
#define DRIVESTAT_VENA                      0x0010   // Voltage enabled
#define DRIVESTAT_QSTOP                     0x0020   // Doing quick stop when CLEAR
#define DRIVESTAT_SOD                       0x0040   // Switch-on disabled
#define DRIVESTAT_WARN                      0x0080   // A warning condition is in effect
#define DRIVESTAT_ABORT                     0x0100   // Trajectory aborted
#define DRIVESTAT_REMOTE                    0x0200   // Controlled by CANopen network if set
#define DRIVESTAT_MOVEDONE                  0x0400   // Target reached
#define DRIVESTAT_LIMIT                     0x0800   // Internal limit active
#define DRIVESTAT_SPACK                     0x1000   // pp mode - setpoint acknowledge
#define DRIVESTAT_TRACKERR                  0x2000   // pp mode - following error
#define DRIVESTAT_REFERENCED                0x1000   // home mode - drive has been referenced
#define DRIVESTAT_HOMEERR                   0x2000   // home mode - homing error
#define DRIVESTAT_TRJ                       0x4000   // Trajectory active when set
#define DRIVESTAT_HOMECAP                   0x8000   // Home position captured (requires 4.77+ firmware)

// Event status bits that are considered amplifier errors
#define ERROR_EVENTS (ESTAT_SHORT_CRCT | ESTAT_AMP_TEMP | ESTAT_MTR_TEMP | ESTAT_ENCODER_PWR | \
                      ESTAT_PHASE_ERR | ESTAT_TRK_ERR | ESTAT_UNDER_VOLT | ESTAT_OVER_VOLT)


/***************************************************************************/
/**
This PDO is used to get status updates from the drive any time the status changes.
*/
/***************************************************************************/
CML_NAMESPACE_START()
class TPDO_Status: public TPDO
{
   bool first;
   uint32 ampRef[ MAX_AXIS ];
   uint16 oldI;
   uint16 oldS[ MAX_AXIS ];
   uint32 oldE[ MAX_AXIS ];
   Pmap16 status[ MAX_AXIS ];                 ///< Used to map the CAN status word passed in the PDO
   Pmap32 estat[ MAX_AXIS ];                  ///< Used to map the 'event status' word to the PDO
   Pmap16 inputs;
   int axisCt;
public:

   /// Default constructor for this PDO
   TPDO_Status( void )
   {
      SetRefName( "TPDO_Status" );
      for( int i=0; i<MAX_AXIS; i++ ) ampRef[i]=0;
   }

   ~TPDO_Status()
   { 
      KillRef();

      for( int i=0; i<MAX_AXIS; i++ )
         RefObj::ReleaseRef( ampRef[i] );
   }

   EVENT_STATUS getPdoEventStat( int n ){ return (EVENT_STATUS)estat[n].Read(); }
   uint16 getCanStat( int n ){ return status[n].Read(); }

   /**
   Initialize a transmit PDO used to send status updates.
   The PDO is initialized to transmit a PDO on status events
   which occur when the amp status changes.

   @param amp Reference to the amplifier object 
   @param slot TPDO slot number for this PDO
   @return An error object
   */
   const Error *Init( Amp &amp, uint16 slot, int numAxes )
   {
      CML_ASSERT( numAxes <= MAX_AXIS );
      if( numAxes > MAX_AXIS ) return &AmpError::BadAxis;
      axisCt = numAxes;

      cml.Debug( "Amp %d Status PDO Init\n", amp.GetNodeID() );
      ampRef[0] = amp.GrabRef();
      first = true;

      // Set transmit type to transmit on events
      const Error *err = SetType( 255 );

      // Let the various mapped variables know which 
      // objects in the amp's object dictionary they
      // are linked to.
      if( !err ) err = inputs.Init( OBJID_INPUTS, 0 );
      if( !err ) err = AddVar( inputs );

      // The drive's event status register was originally mapped to object 0x1002.
      // On later drives that support multi-axis EtherCAT communications, it was also
      // mapped at 0x2185 and offsets thereof.
      uint16 eventStatusObj = (numAxes>1) ? 0x2185 : OBJID_EVENT_STAT;

      for( int i=0; i<numAxes; i++ )
      {
         if( !err ) err = status[i].Init( OBJID_STATUS + 0x800*i, 0 );
         if( !err ) err = estat[i].Init( eventStatusObj + 0x800*i, 0 );

         // Add the mapped variables
         if( !err ) err = AddVar( status[i] );
         if( !err ) err = AddVar( estat[i] );
      }

      // Program this PDO in the amp, and enable it
      if( !err ) err = amp.PdoSet( slot, *this );

      return err;
   }

   /**
   Uninitialize the transmit PDO

   @param amp Reference to the amplifier object 
   @param slot TPDO slot number for this PDO
   @return An error object
   */
   const Error *Uninit( Amp &amp, uint16 slot, int numAxes )
   {
      const Error *err = amp.PdoDisable( slot, *this );
      return err;
   }

   /**
   Handle the reception of status information.  This PDO waits for one of the
   required bits in the status word to be set, and posts new events to the 
   amplifier as they occur.
   */
   void Received( void )
   {
      uint16 ins = inputs.Read();

      for( int i=0; i<axisCt; i++ )
      {
         uint16 s = status[i].Read();
         uint32 e = estat[i].Read();

         if( first || (e!=oldE[i]) || (s!=oldS[i]) || (ins!=oldI) )
         {
            RefObjLocker<Amp> ampPtr( ampRef[i] );
            if( ampPtr )
               ampPtr->UpdateEvents( s, e, ins );
         }

         oldI = ins;
         oldS[i] = s;
         oldE[i] = e;
      }

      first = false;
   }

   const Error *ConnectSubAxis( unsigned int i, Amp *ax )
   {
      CML_ASSERT( i < MAX_AXIS );
      if( i >= MAX_AXIS )
         return &AmpError::BadAxis;
      ampRef[i] = ax->GrabRef();
      return 0;
   }
};

/***************************************************************************/
/**
Receive PDO used to send control word updates.
This is only used on EtherCAT networks
*/
/***************************************************************************/
class RPDO_Ctrl: public RPDO
{
   uint32 netRef;
   Pmap16 ctrl[ MAX_AXIS ];
   int axisCt;

public:
   /// Default constructor for this PDO
   RPDO_Ctrl()
   {
      SetRefName( "RPDO_Ctrl" );
      axisCt = 0;
      netRef = 0;
   }

   virtual ~RPDO_Ctrl()
   { 
      KillRef(); 
      RefObj::ReleaseRef( netRef );
   }

   const Error *Init( Amp &amp, uint16 slot, uint16 numAxes )
   {
      CML_ASSERT( numAxes <= MAX_AXIS );
      if( numAxes > MAX_AXIS ) return &AmpError::BadAxis;
      axisCt = numAxes;

      RefObjLocker<Network> net( amp.GetNetworkRef() );
      if( !net ) return &NodeError::NetworkUnavailable;
      netRef = net->GrabRef();

      const Error *err = SetType( 255 );

      for( int i=0; i<numAxes; i++ )
      {
         uint16 val;
         if( !err ) err = amp.Upld16( OBJID_CONTROL + 0x800*i, 0, val );
         ctrl[i].Write( val );

         if( !err ) err = ctrl[i].Init( OBJID_CONTROL+0x800*i, 0 );
         if( !err ) err = AddVar( ctrl[i] );
      }

      if( !err ) err = amp.PdoSet( slot, *this );

      return err;
   }

   const Error *Uninit( Amp &amp, uint16 slot, int numAxes )
   {
      const Error *err = amp.PdoDisable( slot, *this );
      return err;
   }

   const Error *Transmit( int axis, uint16 val )
   {
      CML_ASSERT( axis < MAX_AXIS );
      if( axis >= MAX_AXIS ) return &AmpError::BadAxis;

      ctrl[axis].Write( val );

      RefObjLocker<Network> net( netRef );
      if( !net ) return &NodeError::NetworkUnavailable;

      return RPDO::Transmit( *net );
   }
};

/**
Transmit PDO used to send out PVT buffer status updates
*/
class TPDO_PvtStat: public TPDO, Thread
{
   bool first;
   Semaphore sem;
   uint32 ampRef[MAX_AXIS];
   Pmap32 stat[MAX_AXIS];
   uint32 newStat[MAX_AXIS];
   int axisCt;

public:
   /// Default constructor for this PDO
   TPDO_PvtStat()
   {
      SetRefName( "TPDO_PvtStat" );
      for( int i=0; i<MAX_AXIS; i++ )
         ampRef[i] = 0;
      axisCt = 0;
   }

   virtual ~TPDO_PvtStat()
   { 
      stop(100);
      KillRef(); 

      for( int i=0; i<MAX_AXIS; i++ )
         RefObj::ReleaseRef( ampRef[i] );
   }

   const Error *Init( Amp &amp, uint16 slot, uint16 numAxes )
   {
      CML_ASSERT( numAxes <= MAX_AXIS );
      if( numAxes > MAX_AXIS ) return &AmpError::BadAxis;
      axisCt = numAxes;

      first = true;

      ampRef[0] = amp.GrabRef();

      const Error *err = SetType( 255 );

      for( int i=0; i<numAxes; i++ )
      {
         if( !err ) err = stat[i].Init( OBJID_PVT_BUFF_STAT + i*0x800, 0 );
         if( !err ) err = AddVar( stat[i] );
      }

      if( !err ) err = amp.PdoSet( slot, *this );
      if( !err ) err = start();

      return err;
   }

   const Error *Uninit( Amp &amp, uint16 slot, int numAxes )
   {
      return amp.PdoDisable( slot, *this );
   }

   void Received( void )
   {
      bool post = first;
      first = false;

      for( int i=0; i<axisCt; i++ )
      {
         uint32 s = stat[i].Read();
         if( s != newStat[i] )
            post = true;
         newStat[i] = s;
      }

      if( post ) sem.Put();
   }

   const Error *ConnectSubAxis( unsigned int i, Amp *ax )
   {
      CML_ASSERT( i < MAX_AXIS );
      if( i >= MAX_AXIS )
         return &AmpError::BadAxis;
      ampRef[i] = ax->GrabRef();
      return 0;
   }

   // FIXME - this is going to create too many threads on a system with lots of drives.
   //         Need to use a single thread to do this for all drives
   void run( void )
   {
      while( 1 )
      {
         const Error *err = sem.Get();
         if( err )
         {
            sleep( 10 );
            continue;
         }

         for( int i=0; i<axisCt; i++ )
         {
            RefObjLocker<Amp> ampPtr( ampRef[i] );
            if( ampPtr )
               ampPtr->PvtStatusUpdate( newStat[i] );
         }
      }
   }
};

/**
Receive PDO used to send PVT segments to the amplifier
This is only used on CANopen networks
*/
class RPDO_PvtCtrl: public RPDO
{
   int axisCt;
   uint32 netRef;
   PmapRaw dat[ MAX_AXIS ];

public:
   /// Default constructor for this PDO
   RPDO_PvtCtrl()
   { 
      SetRefName( "RPDO_Pvt" ); 
      netRef = 0;
   }

   virtual ~RPDO_PvtCtrl()
   { 
      KillRef(); 
      RefObj::ReleaseRef( netRef );
   }

   const Error *Init( Amp &amp, uint16 slot, uint16 numAxes )
   {
      CML_ASSERT( numAxes <= MAX_AXIS );
      if( numAxes > MAX_AXIS ) return &AmpError::BadAxis;
      axisCt = numAxes;

      RefObjLocker<Network> net( amp.GetNetworkRef() );
      if( !net ) return &NodeError::NetworkUnavailable;
      netRef = net->GrabRef();

      const Error *err = SetType( 255 );

      for( int i=0; i<numAxes; i++  )
      {
         if( !err ) err = dat[i].Init( OBJID_PVT_DATA + i*0x800, 0, 64 );
         if( !err ) err = AddVar( dat[i] );
      }

      if( !err ) err = amp.PdoSet( slot, *this );

      return err;
   }

   const Error *Uninit( Amp &amp, uint16 slot, int numAxes )
   {
      const Error *err = amp.PdoDisable( slot, *this );
      return err;
   }

   const Error *Transmit( int axis, byte *data )
   {
      CML_ASSERT( axis < MAX_AXIS );
      if( axis >= MAX_AXIS ) return &AmpError::BadAxis;

      dat[axis].Set(data);

      RefObjLocker<Network> net( netRef );
      if( !net ) return &NodeError::NetworkUnavailable;

      return RPDO::Transmit( *net );
   }
};

CML_NAMESPACE_END()
CML_NAMESPACE_USE();

// Initialize the PDOs used to get status from the drive and send control info to it.
const Error *Amp::InitPDOs( void )
{
   const Error *err = 0;

   // If this is a secondary axis, just attach to the primary axis status PDO
   if( primaryAmpRef )
   {
      RefObjLocker<Amp> pri( primaryAmpRef );
      if( !pri ) return &AmpError::NotInit;
      if( !err ) err = pri->statPDO->ConnectSubAxis( axisNum, this );
      if( !err ) err = pri->pvtStatPDO->ConnectSubAxis( axisNum, this );
      return err;
   }

   // Disable all the default PDOs 
   cml.Debug( "Amp %d, Initting PDOs\n", GetNodeID() );
   for( int i=0; i<8; i++ )
   {
      if( !err ) err = TpdoDisable( i );
      if( !err ) err = RpdoDisable( i );
   }

   // See if I need to init for multiple axes
   int16 numAxes = 1;
   if( GetNetworkType() == NET_TYPE_ETHERCAT )
   {
      err = Upld16( OBJID_AMP_INFO, 25, numAxes );
      if( err ) return err;
   }

   // Release any PDOs that were previously allocated
   FreePDOs();

   statPDO = new TPDO_Status();
   err = statPDO->Init( *this, 0, numAxes );
   if( err )
   {
      delete statPDO;
      statPDO = 0;
      return err;
   }

   pvtStatPDO = new TPDO_PvtStat();
   err = pvtStatPDO->Init( *this, 1, numAxes );
   if( err )
   {
      delete pvtStatPDO;
      pvtStatPDO = 0;
      return err;
   }

   if( GetNetworkType() == NET_TYPE_ETHERCAT )
   {
      ctrlPDO = new RPDO_Ctrl();
      err = ctrlPDO->Init( *this, 0, numAxes );
      if( err )
      {
         delete ctrlPDO;
         ctrlPDO = 0;
         return err;
      }
   }
   else
   {
      pvtCtrlPDO = new RPDO_PvtCtrl();
      err = pvtCtrlPDO->Init( *this, 0, numAxes );
      if( err )
      {
         delete pvtCtrlPDO;
         pvtCtrlPDO = 0;
         return err;
      }
   }

   return 0;
}

// Uninitialize the PDOs used to get status from the drive. 
const Error *Amp::UninitPDOs( void )
{
   const Error *err = 0;

   // If this is a secondary axis, we shouldn't have to uninit the PDOs
   if( primaryAmpRef )
   {
      return err;
   }

   // Disable all the default PDOs 
   cml.Debug( "Amp %d, Uninitting PDOs\n", GetNodeID() );
   // See if I need to uninit for multiple axes
   int16 numAxes = 1;

   if( GetNetworkType() == NET_TYPE_ETHERCAT )
   {
      err = Upld16( OBJID_AMP_INFO, 25, numAxes );
      if( err ) return err;
   }

   if (statPDO)
   {
      err = statPDO->Uninit( *this, 0, numAxes );
      if( err )
      {
         delete statPDO;
         statPDO = 0;
         return err;
      }
   }
   
   if (pvtStatPDO)
   {
      err = pvtStatPDO->Uninit( *this, 1, numAxes );
      if( err )
      {
         delete pvtStatPDO;
         pvtStatPDO = 0;
         return err;
      }
   }

   if( GetNetworkType() == NET_TYPE_ETHERCAT )
   {
      if (ctrlPDO)
      {
         err = ctrlPDO->Uninit( *this, 0, numAxes );
         if( err )
         {
            delete ctrlPDO;
            ctrlPDO = 0;
            return err;
         }
      }
   }
   else
   {
      if (pvtCtrlPDO)
      {
         err = pvtCtrlPDO->Uninit( *this, 0, numAxes );
         if( err )
         {
            delete pvtCtrlPDO;
            pvtCtrlPDO = 0;
            return err;
         }
      }
   }

   return 0;
}


void Amp::FreePDOs( void )
{
   if( statPDO ) delete statPDO;
   if( ctrlPDO ) delete ctrlPDO;
   if( pvtCtrlPDO ) delete pvtCtrlPDO;
   if( pvtStatPDO ) delete pvtStatPDO;
   statPDO = 0;
   ctrlPDO = 0;
   pvtCtrlPDO = 0;
   pvtStatPDO = 0;
}

/***************************************************************************/
/**
Request a status update from the amplifier.  
 */
/***************************************************************************/
const Error *Amp::RequestStatUpdt( void )
{
   // For secondary axis objects, do this through the primary axis
   if( primaryAmpRef )
   {
      RefObjLocker<Amp> pri( primaryAmpRef );
      if( !pri ) return &AmpError::NotInit;
      return pri->RequestStatUpdt();
   }

   // Note that 8367 based products don't support remote 
   // requests well, so we use an SDO to request the PDO
   if( (hwType & 0xFF00) != 0x0300 )
   {
      RefObjLocker<Network> net( GetNetworkRef() );
      if( !net ) return &NodeError::NetworkUnavailable;

      if( !statPDO ) 
         return &AmpError::NotInit;

      return statPDO->Request(*net);
   }
   else
      return Dnld8( OBJID_PDOREQUEST, 0, (uint8)0 );
}


/// Return the most recent event status word from my PDO
EVENT_STATUS Amp::getPdoEventStat( void )
{
   EVENT_STATUS ret = (EVENT_STATUS)0;
   if( primaryAmpRef )
   {
      RefObjLocker<Amp> pri( primaryAmpRef );
      if( pri && pri->statPDO )
         ret = pri->statPDO->getPdoEventStat( axisNum );
   }
   else if( statPDO )
      ret = statPDO->getPdoEventStat(0);
   return ret;
}

/// Return the DS402 status word from my PDO
uint16 Amp::getPdoDS402Stat( void )
{
   uint16 ret = 0;

   if( primaryAmpRef )
   {
      RefObjLocker<Amp> pri( primaryAmpRef );
      if( pri && pri->statPDO )
         ret = pri->statPDO->getCanStat( axisNum );
   }
   else if( statPDO )
      ret = statPDO->getCanStat(0);
   return ret;
}

/***************************************************************************/
/**
Update the amplifier's event map based on the status information received
by a status PDO.  This function is intended for internal use and shouldn't 
generally be called by user code.

@param stat The CANopen status word
@param events The Event status word
@param inputs The current state of the first 16 input pins
@return Null on success, or an error object on failure
*/
/***************************************************************************/
const Error *Amp::UpdateEvents( uint16 stat, uint32 events, uint16 inputs )
{
   cml.Debug( "Amp %d status 0x%08x 0x%04x 0x%04x\n", GetNodeID(), events, stat, inputs );
   uint32 mask = 0;

   if(  stat & DRIVESTAT_SPACK    ) mask |= AMPEVENT_SPACK;
   if(  stat & DRIVESTAT_MOVEDONE ) mask |= AMPEVENT_MOVEDONE;
   if( ~stat & DRIVESTAT_TRJ      ) mask |= AMPEVENT_TRJDONE;
   if( ~stat & DRIVESTAT_QSTOP    ) mask |= AMPEVENT_QUICKSTOP;
   if(  stat & DRIVESTAT_ABORT    ) mask |= AMPEVENT_ABORT;
   if(  stat & DRIVESTAT_HOMECAP  ) mask |= AMPEVENT_HOME_CAPTURE;

   if( events & ERROR_EVENTS      ) mask |= AMPEVENT_ERROR;
   if( events & ESTAT_FAULT       ) mask |= AMPEVENT_FAULT;
   if( events & ESTAT_TRK_WARN    ) mask |= AMPEVENT_POSWARN;
   if( events & ESTAT_TRK_WIN     ) mask |= AMPEVENT_POSWIN;
   if( events & ESTAT_VEL_WIN     ) mask |= AMPEVENT_VELWIN;
   if( events & ESTAT_PWM_DISABLE ) mask |= AMPEVENT_DISABLED;
   if( events & ESTAT_POSLIM      ) mask |= AMPEVENT_POSLIM;    
   if( events & ESTAT_NEGLIM      ) mask |= AMPEVENT_NEGLIM;
   if( events & ESTAT_SOFTLIM_POS ) mask |= AMPEVENT_SOFTLIM_POS;
   if( events & ESTAT_SOFTLIM_NEG ) mask |= AMPEVENT_SOFTLIM_NEG;
   if( events & ESTAT_SOFT_DISABLE) mask |= AMPEVENT_SOFTDISABLE;
   if( events & ESTAT_PHASE_INIT  ) mask |= AMPEVENT_PHASE_INIT;

   // On new move aborts, do some clean up.
   if( mask & AMPEVENT_ABORT )
   {
      uint32 old = eventMap.getMask();

      if( !(old & AMPEVENT_ABORT) )
         MoveAborted();
   }

   // Do the same thing if the amplifier is disabled while 
   // a trajectory was in progress
   if( mask & AMPEVENT_DISABLED )
   {
      uint32 old = eventMap.getMask();

      if( !(old & AMPEVENT_TRJDONE) )
         MoveAborted();
   }

   // Change the bits that this function is responsible for.
   // for now, that's everything but the node guarding 
   // and PVT buffer empty bits.
   eventMap.changeBits( ~(AMPEVENT_PVT_EMPTY|AMPEVENT_NODEGUARD), mask );

   // Update the input pins state
   inputStateMap.setMask( (uint32)inputs );

   return 0;
}

// Send a PDO to update the control word.  Only used on EtherCAT networks.
const Error *Amp::xmitCtrlPDO( uint16 val )
{
   if( primaryAmpRef )
   {
      RefObjLocker<Amp> pri( primaryAmpRef );
      if( pri && pri->ctrlPDO )
         return pri->ctrlPDO->Transmit( axisNum, val );
   }
   else if( ctrlPDO )
      return ctrlPDO->Transmit( 0, val );
   return 0;
}

// Send a PDO to update the PVT buffer
const Error *Amp::xmitPvtPDO( uint8 val[] )
{
   if( primaryAmpRef )
   {
      RefObjLocker<Amp> pri( primaryAmpRef );
      if( pri && pri->pvtCtrlPDO )
         return pri->pvtCtrlPDO->Transmit( axisNum, val );
   }
   else if( pvtCtrlPDO )
      return pvtCtrlPDO->Transmit( 0, val );
   return 0;
}

/***************************************************************************/
/**
Transmit PDO used to send out high resolution time stamp messages.
By default, the synch producer is configured to generate this PDO
every 100 ms.
*/
/***************************************************************************/
class TPDO_HighResTime: public TPDO
{
   Pmap32 stamp;                      ///< Used to map the high resolution time stamp information
public:
   /// Default constructor for a high res timestamp transmit PDO
   TPDO_HighResTime(){}
   const Error *Init( Amp &amp, uint16 slot, uint32 id );
   const Error *Uinit( Amp &amp, uint16 slot, uint32 id );
};

/***************************************************************************/
/**
Receive PDO used to receive high resolution time stamp messages.
By default, all synch consumers are configured to receive this PDO.
*/
/***************************************************************************/
class RPDO_HighResTime: public RPDO
{
   Pmap32 stamp;                      ///< Used to map the high resolution time stamp information
public:
   RPDO_HighResTime(){}
   const Error *Init( Amp &amp, uint16 slot, uint32 id );
};

/***************************************************************************/
/**
Initialize a transmit PDO used to send high resolution time information.
The PDO is initialized to transmit a PDO every 10 SYNCH messages.
@param amp Reference to the amplifier object 
@param slot TPDO slot number for this PDO
@param id The CAN message ID to be used by this PDO.
@return An error object
*/
/***************************************************************************/
const Error *TPDO_HighResTime::Init( Amp &amp, uint16 slot, uint32 id )
{
   // Initialize the transmit PDO
   const Error *err = TPDO::Init( id );

   // Set transmit type to transmit every 10 synch signals
   if( !err ) err = SetType( 10 );

   // Initialize my time stamp variable to identify the
   // high res time stamp object.
   if( !err ) err = stamp.Init( 0x1013, 0 );

   // Add the mapped variable
   if( !err ) err = AddVar( stamp );

   // Program this PDO in the amp, and enable it
   if( !err ) err = amp.PdoSet( slot, *this );

   return err;
}

/***************************************************************************/
/**
Initialize a receive PDO used to receive high resolution time information.
@param amp The amplifier that the PDO is associated with
@param slot The RPDO slot used by this PDO
@param id The CAN ID value to be used by the PDO
@return An error object
*/
/***************************************************************************/
const Error *RPDO_HighResTime::Init( Amp &amp, uint16 slot, uint32 id )
{
   const Error *err = RPDO::Init( id ); 

   if( !err ) err = stamp.Init( 0x1013, 0 );
   if( !err ) err = AddVar( stamp );
   if( !err ) err = SetType( 255 );
   if( !err ) err = amp.PdoSet( slot, *this );

   return err;
}

/***************************************************************************/
/**
Setup the amplifier's high resolution time stamp PDO.  This is used to 
synchronize multiple amplifiers over the CANopen network.

To use this PDO, one amplifier (normally the SYNC producer) is configured 
to transmit the PDO roughly every 100ms.  The other amplifiers are configured
to receive the PDO and adjust their internal clocks based on the time stamp
information is contains.
*/
/***************************************************************************/
const Error *Amp::SetupSynchPDO( AmpSettings &settings )
{
   // This feature can be disabled by setting the ID to zero.
   // It's not clear why that would be useful however.
   if( !settings.timeStampID )
      return 0;

   // If this is the synch producer, init the 
   // high resolution time stamp TPDO
   if( settings.synchProducer )
   {
      TPDO_HighResTime pdo;
      return pdo.Init( *this, 4, settings.timeStampID );
   }

   // Otherwise, init an RPDO for this purpose
   else
   {
      RPDO_HighResTime pdo;
      return pdo.Init( *this, 4, settings.timeStampID );
   }
}
