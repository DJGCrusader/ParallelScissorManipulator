/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2010 Copley Controls                  */
/*                     A Division of Analogic           */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/*  CAN Object for VCI V3 Driver                        */
/********************************************************/


#include "vcinpl.h"
#include <process.h>
#include <windows.h>

#include "CML.h"
#include "vcinpl.h"
#include "can_ixxat_v3.h"


CML_NAMESPACE_USE();


/* Types used to define pointers to functions contained in .dll file */
typedef HRESULT ( VCIAPI *vciEnumDeviceOpenType      )( PHANDLE );
typedef HRESULT ( VCIAPI *vciEnumDeviceNextType      )( HANDLE, PVCIDEVICEINFO );
typedef HRESULT ( VCIAPI *vciEnumDeviceCloseType     )( HANDLE );
typedef HRESULT ( VCIAPI *canControlStartType        )( HANDLE, BOOL );
typedef HRESULT ( VCIAPI *canChannelActivateType     )( HANDLE, BOOL );
typedef HRESULT ( VCIAPI *canControlOpenType         )( HANDLE, UINT32, PHANDLE );
typedef HRESULT ( VCIAPI *canControlInitializeType   )( HANDLE, UINT8, UINT8, UINT8 );
typedef HRESULT ( VCIAPI *canChannelOpenType         )( HANDLE, UINT32, BOOL, PHANDLE );
typedef HRESULT ( VCIAPI *canChannelCloseType        )( HANDLE );
typedef HRESULT ( VCIAPI *canControlCloseType        )( HANDLE );
typedef HRESULT ( VCIAPI *vciDeviceOpenType          )( REFVCIID, PHANDLE );
typedef HRESULT ( VCIAPI *vciDeviceCloseType         )( HANDLE );
typedef HRESULT ( VCIAPI *canChannelSendMessageType  )( HANDLE, UINT32, PCANMSG );
typedef HRESULT ( VCIAPI *canChannelReadMessageType  )( HANDLE, UINT32, PCANMSG );
typedef HRESULT ( VCIAPI *canControlSetAccFilterType )( HANDLE, BOOL, UINT32, UINT32 );
typedef HRESULT ( VCIAPI *canChannelInitializeType   )( HANDLE, UINT16, UINT16, UINT16, UINT16 );
typedef HRESULT ( VCIAPI *canControlResetType        )( HANDLE );

/* local functions */
static const Error *InitLibrary( void );
static void UninitLibrary( void );

/* local data */
#define MAX_BOARDS       4
static LONG  lCtrlNo;  // controller number
static int i;          // board number

static IxxatCANV3 *board[ MAX_BOARDS ] = {0};
static Mutex boardMutex;
static Mutex libraryMutex;
static int openCards = 0;

/* local handles */
static HANDLE  hDevice;      // device handle
static HANDLE  hCanCtl;      // controller handle
static HANDLE  hCanChn;      // channel handle
static HMODULE hVCINPL;      // library load handle

/* V3 definitions for pointers to .dll functions */

static vciEnumDeviceOpenType      lp_vciEnumDeviceOpen;
static vciEnumDeviceNextType      lp_vciEnumDeviceNext;
static vciEnumDeviceCloseType     lp_vciEnumDeviceClose;
static canControlStartType        lp_canControlStart;
static canChannelActivateType     lp_canChannelActivate;
static canControlOpenType         lp_canControlOpen;
static canControlInitializeType   lp_canControlInitialize;
static canChannelOpenType         lp_canChannelOpen;
static canChannelCloseType        lp_canChannelClose;
static canControlCloseType        lp_canControlClose;
static vciDeviceOpenType          lp_vciDeviceOpen;
static vciDeviceCloseType         lp_vciDeviceClose;
static canChannelSendMessageType  lp_canChannelSendMessage;
static canChannelReadMessageType  lp_canChannelReadMessage;
static canControlSetAccFilterType lp_canControlSetAccFilter;
static canChannelInitializeType   lp_canChannelInitialize;
static canControlResetType        lp_canControlReset;

/***************************************************************************/
/*                                                                         */
/* Construct a new Ixxat CAN interface object.                             */
/* This simply sets the default baud rate and                              */
/* marks the card as not open.                                             */
/*                                                                         */
/***************************************************************************/
IxxatCANV3::IxxatCANV3( void ) : CanInterface()
{
   // Default baud to 1,000,000 bps
   IxxatCANV3::SetBaud( 1000000 );

   // Default to not open
   open = 0;
}

/***************************************************************************/
/*                                                                         */
/* Construct a new Ixxat CAN interface object for the specified port.      */
/*                                                                         */
/* Port name should be of the form CANx or IXXATx where x is port number.  */
/* The port numbers start at 0, so the first port would be identified by   */
/* the port name CAN0.                                                     */
/*                                                                         */
/* @param port The port name string identifying the CAN device.            */
/*                                                                         */
/***************************************************************************/
IxxatCANV3::IxxatCANV3( const char *port ) : CanInterface(port)
{
   // Default baud to 1,000,000 bps
   IxxatCANV3::SetBaud( 1000000 );

   // Default to not open
   open = 0;
}

/***************************************************************************/
/*                                                                         */
/* Destructor for Ixxat card. Closes the interface.                        */
/*                                                                         */
/***************************************************************************/
IxxatCANV3::~IxxatCANV3(void)
{
   Close();
}

/***************************************************************************/
/*                                                                         */
/* Open the Ixxat CAN card.                                                */
/*                                                                         */
/* The card should have been identified by setting it's name either        */
/* in the constructor, or by using the method CanInterface::SetName.       */
/*                                                                         */
/* If no port name was set, then the default Ixxat card will be used.      */
/*                                                                         */
/* If the port name is set to "select", then a dialog box will be shown    */
/* allowing the card to be selected from any installed Ixxat cards.        */
/*                                                                         */
/* Otherwise, the port name should be of the form "CANx" where x is the    */
/* Ixxat hardware key number (i.e. CAN1 for hardware key 1).               */
/*                                                                         */
/* @return A pointer to an error object on failure, or NULL on success.    */
/*                                                                         */
/***************************************************************************/
const Error *IxxatCANV3::Open( void )
{
   int port;
   HRESULT  hResult;     // error code
   HANDLE   hEnum;       // enumerator handle
   VCIDEVICEINFO  sInfo;  // device info

   mutex.Lock();

   if( open )
   {
      mutex.Unlock();
      return &CanError::AlreadyOpen;
   }

   /**************************************************
    * Find the port number to open.
    **************************************************/
   port = FindPortNumber( "CAN" );
   if( port < 0 ) 
      port = FindPortNumber( "IXXATV3" );

   if( port < 0 )
   {
      mutex.Unlock();
      return &CanError::BadPortName;
   }

   const Error *err = InitLibrary();
   if( err )
   {
      cml.Error( "IxxatCANV3::InitLibrary failed with error %s\n", err->toString() );
      mutex.Unlock();
      return err;
   }

   /**************************************************
    * Find the board configuration info based on the
    * port name specified
    **************************************************/

   /* open the device list */
   hResult = lp_vciEnumDeviceOpen( &hEnum );

   /* retrieve device data */
   if( hResult == VCI_OK )
   {
      hResult = lp_vciEnumDeviceNext( hEnum, &sInfo );
   }

   /* close the device list */
   lp_vciEnumDeviceClose( hEnum );

   /* open the device REQUIRED A CHANGE TO VCITYPE.H!!!!*/
   if( hResult == VCI_OK)
   {
      hResult = lp_vciDeviceOpen( sInfo.VciObjectId, &hDevice );
   }

   /* always select controller 0 */
   lCtrlNo = 0;

   /**************************************************
    * The following was all taken from the InitSocket
    * portion of the V3 example
    **************************************************/


   /* create a message channel */
   if (hDevice != NULL)
   {
      /* create a message */
      hResult = lp_canChannelOpen(hDevice, lCtrlNo, FALSE, &hCanChn);

      /* initialize the message */
      if (hResult == VCI_OK)
      {
         UINT16 wRxFifoSize  = 1024;
         UINT16 wRxThreshold = 1;
         UINT16 wTxFifoSize  = 128;
         UINT16 wTxThreshold = 1;

         hResult = lp_canChannelInitialize( hCanChn, 
                                            wRxFifoSize, 
                                            wRxThreshold, 
                                            wTxFifoSize, 
                                            wTxThreshold );
      }

      /* activate the channel */
      if (hResult == VCI_OK)
      {
         hResult = lp_canChannelActivate(hCanChn, TRUE);
      }

      /* open the CAN controller */
      if (hResult == VCI_OK)
      {
         hResult = lp_canControlOpen(hDevice, lCtrlNo, &hCanCtl);
         /* this function fails if the controller is in use */
         /* by another application. */
      }

      /* initialize the CAN controller */
      if (hResult == VCI_OK)
      {
         hResult = lp_canControlInitialize( hCanCtl, 
                                            CAN_OPMODE_STANDARD |
                                            CAN_OPMODE_ERRFRAME,
                                            CAN_BT0, 
                                            CAN_BT1 );
      }

      /* set acceptance filter */
      if (hResult == VCI_OK)
      { 
         hResult = lp_canControlSetAccFilter( hCanCtl,
                                              CAN_FILTER_STD,
                                              CAN_ACC_CODE_ALL,
                                              CAN_ACC_MASK_ALL );
      }

      /* start the CAN controller */
      if (hResult == VCI_OK)
      {
         hResult = lp_canControlStart(hCanCtl, TRUE);
      }
   }

   else
   {
      hResult = VCI_E_INVHANDLE;
   }

   if( hResult != VCI_OK ) 
      err = ConvertError( hResult );


   if( err )
   {
      if( i >= 0 )
      {
         boardMutex.Lock();
         board[i] = NULL;
         boardMutex.Unlock();
      }
      UninitLibrary();
   }
   else
      open = 1;

   mutex.Unlock();

   return err;
}

/***************************************************************************/
/**
Close the CAN interface.
@return A pointer to an error object on failure, or NULL on success.
*/
/***************************************************************************/
const Error *IxxatCANV3::Close( void )
{
   mutex.Lock();
   if( open )
   {
      lp_canControlReset(hCanCtl);
      lp_canChannelClose(hCanChn);
      lp_canControlClose(hCanCtl);

      lp_vciDeviceClose(hDevice);

      boardMutex.Lock();
      for( int j=0; j<MAX_BOARDS; j++ )
      {
         if( board[j] == this )
            board[j] = NULL;
      }
      boardMutex.Unlock();

      open = 0;
      UninitLibrary();
   }
   mutex.Unlock();
   return 0;
}

/***************************************************************************/
/**
Set the CAN interface baud rate.
@param b The baud rate to set.
@return A pointer to an error object on failure, or NULL on success.
*/
/***************************************************************************/
const Error *IxxatCANV3::SetBaud( int32 b )
{
   switch( b )
   {
      case   10000: CAN_BT0=0x31; CAN_BT1=0x1C; break;
      case   20000: CAN_BT0=0x18; CAN_BT1=0x1C; break;
      case   50000: CAN_BT0=0x09; CAN_BT1=0x1C; break;
      case  100000: CAN_BT0=0x04; CAN_BT1=0x1C; break;
      case  125000: CAN_BT0=0x03; CAN_BT1=0x1C; break;
      case  250000: CAN_BT0=0x01; CAN_BT1=0x1C; break;
      case  500000: CAN_BT0=0x00; CAN_BT1=0x1C; break;
      case  800000: CAN_BT0=0x00; CAN_BT1=0x16; break;
      case 1000000: CAN_BT0=0x00; CAN_BT1=0x14; break;
      default: return &CanError::BadParam;
   }

   baud = b;
   return 0;
}

/***************************************************************************/
/**
Receive the next CAN frame.  
@param frame A reference to the frame object that will be filled by the read.
@param timeout The timeout (ms) to wait for the frame.  A timeout of 0 will
       return immediately if no data is available.  A timeout of < 0 will 
       wait forever.
@return A pointer to an error object on failure, or NULL on success.
*/
/***************************************************************************/
const Error *IxxatCANV3::RecvFrame( CanFrame &frame, Timeout timeout )
{
   HRESULT hResult;
   CANMSG sCanMsg;

   if( !open )
      return &CanError::NotOpen;

   mutex.Lock();

   /* read a CAN message from the receive FIFO */
   hResult = lp_canChannelReadMessage( hCanChn, 1, &sCanMsg);

   frame.id = sCanMsg.dwMsgId;
   frame.length = sCanMsg.uMsgInfo.Bits.dlc;

   //switch( sCanMsg.uMsgInfo.Bytes.bType )
   switch( sCanMsg.uMsgInfo.Bits.rtr )
   {
      //case CAN_MSGTYPE_DATA:
      case FALSE:
         frame.type = CAN_FRAME_DATA;
         break;
         //case CAN_MSGTYPE_INFO:
      case TRUE:
         frame.type = CAN_FRAME_REMOTE;
         break;
      default:
         mutex.Unlock();
         return &CanError::BadParam;
   }

   for( int j=0; j<8; j++)
      frame.data[j] = sCanMsg.abData[j];

   mutex.Unlock();
   return ConvertError( hResult );
}

/***************************************************************************/
/**
Write a CAN frame to the CAN network.
@param frame A reference to the frame to write.
@param timeout The time to wait for the frame to be successfully sent.
       If the timeout is 0, the frame is written to the output queue and
       the function returns without waiting for it to be sent.
       If the timeout is <0 then the function will delay forever.
@return A pointer to an error object on failure, or NULL on success.
*/
/***************************************************************************/
const Error *IxxatCANV3::XmitFrame( CanFrame &frame, Timeout timeout )
{
   HRESULT hResult;

   // don't allow frame lengths longer than 8
   if( frame.length > 8 )
      return &CanError::BadParam;

   if( !open )
   {
      mutex.Unlock();
      return &CanError::NotOpen;
   }

   mutex.Lock();

   CANMSG can_frame;

   /* takes in object, so must be inputted one by one */
   can_frame.dwTime   = 0;

   can_frame.uMsgInfo.Bytes.bFlags = CAN_MAKE_MSGFLAGS(frame.length,0,0,0,0);

   can_frame.dwMsgId = frame.id;

   can_frame.uMsgInfo.Bytes.bType = CAN_MSGTYPE_DATA;
   can_frame.uMsgInfo.Bits.rtr = frame.type;

   for(int j=0; j<can_frame.uMsgInfo.Bits.dlc; j++)
      can_frame.abData[j] = frame.data[j];

   hResult = lp_canChannelSendMessage( hCanChn, 0, &can_frame );

   mutex.Unlock();

   return ConvertError( hResult );

}

/***************************************************************************/
/**
Convert error codes defined by the Vector CAN library into 
the standard error codes used by the motion library.
@param err The Vector style status code
@return A pointer to an error object, or NULL if no error is indicated
*/
/***************************************************************************/
const Error *IxxatCANV3::ConvertError( int err )
{
   switch( err )
   {
      case VCI_OK:                           return 0;
      case (SEV_VCI_ERROR | VCI_E_TIMEOUT):  return &CanError::Timeout;     // Timeout ocurred
      default:                               return &CanError::Driver;
   }
}

/***************************************************************************/
/**
First checks to see if V3 is available otherwise checks to see if V2 is 
available.
Initialize the Ixxat .dll function pointers.  This internal method is called
at startup and used to initialize some local pointers to functions in the 
Ixxat supplied .dll files.
@return A pointer to an error object or NULL on success.
*/
/***************************************************************************/
static const Error *InitLibrary( void )
{
   const Error *err = 0;
   libraryMutex.Lock();

   if( !openCards )
   {
      // Load the Ixxat V3 .dll file
      hVCINPL = LoadLibrary("vcinpl.dll");

      if( !hVCINPL )
      {
         err = &CanError::NoDriver;
         goto done;
      }

      lp_vciEnumDeviceOpen      = (vciEnumDeviceOpenType     )GetProcAddress( hVCINPL, "vciEnumDeviceOpen" );
      lp_vciEnumDeviceNext      = (vciEnumDeviceNextType     )GetProcAddress( hVCINPL, "vciEnumDeviceNext" );
      lp_vciEnumDeviceClose     = (vciEnumDeviceCloseType    )GetProcAddress( hVCINPL, "vciEnumDeviceClose" );
      lp_canControlStart        = (canControlStartType       )GetProcAddress( hVCINPL, "canControlStart" );
      lp_canChannelActivate     = (canChannelActivateType    )GetProcAddress( hVCINPL, "canChannelActivate");
      lp_canControlOpen         = (canControlOpenType        )GetProcAddress( hVCINPL, "canControlOpen");
      lp_canControlInitialize   = (canControlInitializeType  )GetProcAddress( hVCINPL, "canControlInitialize" );
      lp_canChannelClose        = (canChannelCloseType       )GetProcAddress( hVCINPL, "canChannelClose" );
      lp_canChannelOpen         = (canChannelOpenType        )GetProcAddress( hVCINPL, "canChannelOpen" );
      lp_canControlClose        = (canControlCloseType       )GetProcAddress( hVCINPL, "canControlClose" );
      lp_vciDeviceOpen          = (vciDeviceOpenType         )GetProcAddress( hVCINPL, "vciDeviceOpen" );
      lp_vciDeviceClose         = (vciDeviceCloseType        )GetProcAddress( hVCINPL, "vciDeviceClose" );
      lp_canChannelSendMessage  = (canChannelSendMessageType )GetProcAddress( hVCINPL, "canChannelSendMessage" );
      lp_canChannelReadMessage  = (canChannelReadMessageType )GetProcAddress( hVCINPL, "canChannelReadMessage" );
      lp_canControlSetAccFilter = (canControlSetAccFilterType)GetProcAddress( hVCINPL, "canControlSetAccFilter" );
      lp_canChannelInitialize   = (canChannelInitializeType  )GetProcAddress( hVCINPL, "canChannelInitialize" );
      lp_canControlReset        = (canControlResetType       )GetProcAddress( hVCINPL, "canControlReset" );

      if( !lp_vciEnumDeviceOpen      || !lp_vciEnumDeviceNext      || !lp_vciEnumDeviceClose     ||
          !lp_canControlStart        || !lp_canChannelActivate     || !lp_canControlOpen         ||
          !lp_canControlInitialize   || !lp_canChannelClose        || !lp_canChannelOpen         ||
          !lp_canControlClose        || !lp_vciDeviceOpen          || !lp_vciDeviceClose         ||
          !lp_canChannelSendMessage  || !lp_canChannelReadMessage  || !lp_canControlSetAccFilter ||
          !lp_canChannelInitialize   || !lp_canControlReset        )
      {
         err = &CanError::NoDriver;
         FreeLibrary( hVCINPL );
      }
   }

   if( !err )
      openCards++;

done:
   libraryMutex.Unlock();
   return err;
}

/***************************************************************************/
/**
Free the library pointers if they are no longer accessed.
@return A pointer to an error object or NULL on success
*/
/***************************************************************************/
static void UninitLibrary( void )
{
   libraryMutex.Lock();
   if( --openCards == 0 )
   {
      FreeLibrary( hVCINPL);
   }
   libraryMutex.Unlock();
}
