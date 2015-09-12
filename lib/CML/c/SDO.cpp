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

This file contains the code used to implement the CANopen SDO objects.
*/
#include "CML.h"

#define MAX_SDO_LEN 512

CML_NAMESPACE_USE();

/**************************************************
* SDO Error objects
**************************************************/
CML_NEW_ERROR( SDO_Error, NoAbortCode,       "SDO Aborted" );
CML_NEW_ERROR( SDO_Error, Togglebit,         "SDO Abort: Toggle bit not alternated." );
CML_NEW_ERROR( SDO_Error, Timeout,           "SDO Abort: SDO Protocol timed out." );
CML_NEW_ERROR( SDO_Error, Bad_scs,           "SDO Abort: Client/Server command specifier not known." );
CML_NEW_ERROR( SDO_Error, Block_size,        "SDO Abort: Invalid block size." );
CML_NEW_ERROR( SDO_Error, Block_seq,         "SDO Abort: Invalid block sequence." );
CML_NEW_ERROR( SDO_Error, Block_crc,         "SDO Abort: CRC error." );
CML_NEW_ERROR( SDO_Error, Memory,            "SDO Abort: Memory allocation error." );
CML_NEW_ERROR( SDO_Error, Access,            "SDO Abort: Unsupported object access." );
CML_NEW_ERROR( SDO_Error, Writeonly,         "SDO Abort: Object is write only." );
CML_NEW_ERROR( SDO_Error, Readonly,          "SDO Abort: Object is read only." );
CML_NEW_ERROR( SDO_Error, Bad_object,        "SDO Abort: Object does not exist." );
CML_NEW_ERROR( SDO_Error, Pdo_map,           "SDO Abort: Object can not be mapped to PDO." );
CML_NEW_ERROR( SDO_Error, Pdo_length,        "SDO Abort: PDO length would be exceeded." );
CML_NEW_ERROR( SDO_Error, Bad_param,         "SDO Abort: General parameter error." );
CML_NEW_ERROR( SDO_Error, Incompatible,      "SDO Abort: General internal incompatibility." );
CML_NEW_ERROR( SDO_Error, Hardware,          "SDO Abort: Hardware failure." );
CML_NEW_ERROR( SDO_Error, Bad_length,        "SDO Abort: Data length incorrect." );
CML_NEW_ERROR( SDO_Error, Too_long,          "SDO Abort: Data length too long." );
CML_NEW_ERROR( SDO_Error, Too_short,         "SDO Abort: Data length too short." );
CML_NEW_ERROR( SDO_Error, Subindex,          "SDO Abort: Sub-index does not exist." );
CML_NEW_ERROR( SDO_Error, Param_range,       "SDO Abort: Parameter range error." );
CML_NEW_ERROR( SDO_Error, Param_high,        "SDO Abort: Parameter value too high." );
CML_NEW_ERROR( SDO_Error, Param_low,         "SDO Abort: Parameter value too low." );
CML_NEW_ERROR( SDO_Error, Min_max,           "SDO Abort: Maximum value less then minimum." );
CML_NEW_ERROR( SDO_Error, General,           "SDO Abort: General error." );
CML_NEW_ERROR( SDO_Error, Transfer,          "SDO Abort: Data transfer error." );
CML_NEW_ERROR( SDO_Error, Transfer_Local,    "SDO Abort: Data transfer error; local control." );
CML_NEW_ERROR( SDO_Error, Transfer_State,    "SDO Abort: Data transfer error; device state." );
CML_NEW_ERROR( SDO_Error, OD_Gen_Fail,       "SDO Abort: Object dictionary generation failure." );
CML_NEW_ERROR( SDO_Error, Unknown,           "SDO Abort: Unknown abort code" );
CML_NEW_ERROR( SDO_Error, NoBlkXfers,        "Network does not support block transfers" );
CML_NEW_ERROR( SDO_Error, ObjMapActive,      "SDO Abort: sync manager mapping can't be changed while active" );

/***************************************************************************/
/**
Default SDO object constructor.  The SDO must be initialized by calling
SDO::Init before it's actually used.
*/
/***************************************************************************/
SDO::SDO( void )
{
   node     = 0;
   timeout  = 2000;
   maxRetry = 4;
}

/***************************************************************************/
/**
Initialize a CANopen Service Data Object (SDO).
@param n Pointer to the node that this SDO is associated with.
@param to The timeout (milliseconds) for use with this SDO.
@return A valid CANopen error object
*/
/***************************************************************************/
const Error *SDO::Init( Node *n, Timeout to )
{
   // NOTE: The node may not have been attached to it's network when this
   //       function is called.
   node     = n;
   timeout  = to;
   maxRetry = 4;

   blkUpldOK = false;
   blkDnldOK = false;

   return 0;
}

/***************************************************************************/
/**
Enable the use of block uploads with this SDO object
@return A CANopen error object, or null on success.
*/
/***************************************************************************/
const Error *SDO::EnableBlkUpld( void )
{
   if( !node ) return &CanOpenError::NotInitialized;

   if( node->GetNetworkType() != NET_TYPE_CANOPEN )
      return &SDO_Error::NoBlkXfers;

   blkUpldOK = true;
   return 0;
}

/***************************************************************************/
/**
Disable the use of block uploads with this SDO object
@return A CANopen error object, or null on success.
*/
/***************************************************************************/
const Error *SDO::DisableBlkUpld( void )
{
   blkUpldOK = false;
   return 0;
}

/***************************************************************************/
/**
Enable the use of block downloads with this SDO object
@return A CANopen error object, or null on success.
*/
/***************************************************************************/
const Error *SDO::EnableBlkDnld( void )
{
   if( !node ) return &CanOpenError::NotInitialized;

   if( node->GetNetworkType() != NET_TYPE_CANOPEN )
      return &SDO_Error::NoBlkXfers;

   blkDnldOK = true;
   return 0;
}

/***************************************************************************/
/**
Disable the use of block downloads with this SDO object
@return A CANopen error object, or null on success.
*/
/***************************************************************************/
const Error *SDO::DisableBlkDnld( void )
{
   blkDnldOK = false;
   return 0;
}

/***************************************************************************/
/**
Download a 32-bit value using this SDO.
@param index The index of the object in the object dictionary
@param sub The sub-index of the object in the object dictionary
@param data The data to be downloaded
@return A valid CANopen error code.
*/
/***************************************************************************/
const Error *SDO::Dnld32( int16 index, int16 sub, uint32 data )
{
   byte buff[4];
   buff[0] = ByteCast(data);
   buff[1] = ByteCast(data>>8);
   buff[2] = ByteCast(data>>16);
   buff[3] = ByteCast(data>>24);
   return Download( index, sub, 4, buff );
}

/***************************************************************************/
/**
Upload a 32-bit value using this SDO.
@param index The index of the object in the object dictionary
@param sub The sub-index of the object in the object dictionary
@param data The uploaded data will be returned here.
@return A valid CANopen error code.
*/
/***************************************************************************/
const Error *SDO::Upld32( int16 index, int16 sub, uint32 &data )
{
   int32 size = 4;
   byte buff[4];
   buff[0] = buff[1] = buff[2] = buff[3] = 0;

   const Error *err = Upload( index, sub, size, buff );

   data = bytes_to_uint32( buff );

   return err;
}

/***************************************************************************/
/**
Download a 16-bit value using this SDO.
@param index The index of the object in the object dictionary
@param sub The sub-index of the object in the object dictionary
@param data The data to be downloaded
@return A valid CANopen error code.
*/
/***************************************************************************/
const Error *SDO::Dnld16( int16 index, int16 sub, uint16 data )
{
   byte buff[2];
   buff[0] = ByteCast(data);
   buff[1] = ByteCast(data>>8);
   return Download( index, sub, 2, buff );
}

/***************************************************************************/
/**
Upload a 16-bit value using this SDO.
@param index The index of the object in the object dictionary
@param sub The sub-index of the object in the object dictionary
@param data The uploaded data will be returned here.
@return A valid CANopen error code.
*/
/***************************************************************************/
const Error *SDO::Upld16( int16 index, int16 sub, uint16 &data )
{
   int32 size = 2;
   byte buff[2];
   buff[0] = buff[1] = 0;

   const Error *err = Upload( index, sub, size, buff );

   data = bytes_to_uint16(buff);
   return err;
}

/***************************************************************************/
/**
Download a 8-bit value using this SDO.
@param index The index of the object in the object dictionary
@param sub The sub-index of the object in the object dictionary
@param data The data to be downloaded
@return A valid CANopen error code.
*/
/***************************************************************************/
const Error *SDO::Dnld8( int16 index, int16 sub, uint8 data )
{
   return Download( index, sub, 1, &data );
}

/***************************************************************************/
/**
Upload a 8-bit value using this SDO.
@param index The index of the object in the object dictionary
@param sub The sub-index of the object in the object dictionary
@param data The uploaded data will be returned here.
@return A valid CANopen error code.
*/
/***************************************************************************/
const Error *SDO::Upld8( int16 index, int16 sub, uint8 &data )
{
   int32 size = 1;
   return Upload( index, sub, size, &data );
}

/***************************************************************************/
/**
Download a visible string type using the SDO.  The string is assumed
to be null terminated.

@param index The index of the object in the object dictionary
@param sub The sub-index of the object in the object dictionary
@param data A null terminated string to be downloaded.
@return A valid CANopen error code.
*/
/***************************************************************************/
const Error *SDO::DnldString( int16 index, int16 sub, char *data )
{
   // Find the string length
   int32 i;
   for( i=0; data[i]; i++ ){}

   // Also download the null byte
   i++;

   return Download( index, sub, i, data );
}

/***************************************************************************/
/**
Upload a visible string type from the SDO.  The only difference between
this function and the lower level Upload function is that this function
guarantees that there will be a zero character at the end of the string.

@param index The index of the object in the object dictionary
@param sub The sub-index of the object in the object dictionary
@param len Holds the size of the buffer on entry, and the
           length of the downloaded data on return.
@param data The uploaded string will be returned here.
@return A valid CANopen error code.
*/
/***************************************************************************/
const Error *SDO::UpldString( int16 index, int16 sub, int32 &len, char *data )
{
   len--;

   const Error *err = Upload( index, sub, len, data );
   if( err ) return err;

   data[len] = 0;
   return 0;
}

/***************************************************************************/
/**
Download data using this SDO.  The passed array of data is downloaded to the
object dictionary of a node on the network using this SDO.
@param index The index of the object to be downloaded.
@param sub The sub-index of the object to be downloaded.
@param size The number of bytes of data to be downloaded
@param data A character array holding the data to be downloaded.
@return A valid CANopen error object.
*/
/***************************************************************************/
const Error *SDO::Download( int16 index, int16 sub, int32 size, byte *data )
{
   const Error *err = 0;

   for( uint8 i=0; i<maxRetry; i++ )
   {
      err = _Download( index, sub, size, data );
      if( !err ) return 0;

      cml.Debug( "Error (%s) on SDO download attempt %d to 0x%04x.%d, retrying...\n", err->toString(), i, index, sub );
   }

   return err;
}

/***************************************************************************/
/**
Download data using this SDO.  This internal function makes a single download attempt;
@param index The index of the object to be downloaded.
@param sub The sub-index of the object to be downloaded.
@param size The number of bytes of data to be downloaded
@param data A character array holding the data to be downloaded.
@return A valid CANopen error object.
*/
/***************************************************************************/
const Error *SDO::_Download( int16 index, int16 sub, int32 size, byte *data )
{
   const Error *err;
   uint8 buff[MAX_SDO_LEN];

   // Make sure the SDO has been initialized
   if( !node ) return &CanOpenError::NotInitialized;

   // Check for a reasonable size
   if( size <= 0 ) return &CanOpenError::BadParam;

   // Find the maximum number of bytes I can send to the drive.
   // This is 8 for CANopen, and the mailbox size for EtherCAT
   int maxXfer = node->maxSdoToNode();
   if( maxXfer > MAX_SDO_LEN ) maxXfer = MAX_SDO_LEN;
   if( !maxXfer ) return &CanOpenError::NotInitialized;

   // The minimum size I support for SDO transfers is 8 bytes.
   // That's the normal size for CANopen.  For EtherCAT, the 
   // mailbox is normally much larger then this.
   CML_ASSERT( maxXfer >= 8 );

   //FIXME: Not implemented, Copley drives have no current use for it
   // Use a block download if it makes sense to do so
   //if( blkDnldOK && size >= SDO_BLK_DNLD_THRESHOLD )
   //   return BlockDnld( index, sub, size, data );

   MutexLocker ml( mutex );

   // send an "Initiate SDO download" message.

   // Copy the object multiplexor to the frame
   // and also make a local copy.
   buff[1] = mplex[0] = ByteCast(index);
   buff[2] = mplex[1] = ByteCast(index>>8);
   buff[3] = mplex[2] = ByteCast(sub);

   // If the data size is <= 4 bytes, then send an expedited download
   int32 remain, xmitSize;
   if( size <= 4 )
   {
      buff[0] = 0x23 | ((4-size)<<2);

      int32 i;
      for( i=0; i<size; i++ ) buff[i+4] = ByteCast( data[i] );
      for( ; i<4; i++ )       buff[i+4] = 0;
      xmitSize = 8;
      remain = 0;
   }

   // Otherwise, send a normal init
   else
   {
      buff[0] = 0x21;
      int32_to_bytes( size, &buff[4] );

      int32 i, ct = size;
      if( size > (maxXfer-8) ) ct = maxXfer-8;

      for( i=0; i<ct; i++ )
         buff[i+8] = ByteCast( *data++ );

      remain = size - ct;
      xmitSize = ct+8;
   }

   uint16 retLen = MAX_SDO_LEN;
   err = XmitSDO( buff, xmitSize, &retLen, timeout );
   if( err ) return SendAbort( err );

   // Check for abort
   int scs = 7 & (buff[0]>>5);
   if( scs == 4 ) return getAbortRcvdErr( buff );

   // I expect an scs value of 3 (init download response).
   if( scs != 3 )
      return SendAbort( &CanOpenError::SDO_BadMsgRcvd );

   // If the multiplexor was not right, abort the transfer
   if( (buff[1] != mplex[0]) || (buff[2] != mplex[1]) || (buff[3] != mplex[2]) )
      return SendAbort( &CanOpenError::SDO_BadMuxRcvd );

   // Keep sending data until we're done
   int toggle = 0;
   while( remain )
   {
      // Send the next data block.
      int ct = (remain > (maxXfer-1) ) ? (maxXfer-1) : remain;
      remain -= ct;

      buff[0] = (ct<7) ? ((7-ct)<<1) : 0;
      if( toggle )  buff[0] |= 0x10;
      if( !remain ) buff[0] |= 0x01;

      for( int i=1; i<=ct; i++ )
         buff[i] = ByteCast(*data++);

      toggle = !toggle;
      retLen = MAX_SDO_LEN;
      xmitSize = (ct<7) ? 8 : (ct+1);
      err = XmitSDO( buff, xmitSize, &retLen, timeout );
      if( err ) return SendAbort( err );

      scs = 7 & (buff[0]>>5);

      // Check for received aborts
      if( scs == 4 ) return getAbortRcvdErr( buff );

      // I expect an scs value of 1 (download response).
      if( scs != 1 )
         return SendAbort( &CanOpenError::SDO_BadMsgRcvd );

      // Check the toggle bit
      int expected = toggle ? 0 : 0x10;
      if( (buff[0] & 0x10) != expected )
         return SendAbort( &SDO_Error::Togglebit );
   }

   return 0;
}

/***************************************************************************/
/**
Upload data using this SDO.  The value of the object is uploaded from the
object dictionary of a node on the CANopen network using this SDO.  The 
results of the upload are stored in the passed buffer.

@param index The index of the object to be uploaded.
@param sub The sub-index of the object to be uploaded.
@param size On entry, this gives the maximum number of bytes of data to 
       be uploaded.  On successful return, it gives the actual number of bytes received.
@param data A character array which will store the uploaded data.
@return A valid CANopen error object.
*/
/***************************************************************************/
const Error *SDO::Upload( int16 index, int16 sub, int32 &size, byte *data )
{
   const Error *err = 0;
   int32 max = size;

   for( uint8 i=0; i<maxRetry; i++ )
   {
      err = _Upload( index, sub, size, data );
      if( !err ) return 0;
      cml.Debug( "Error (%s) on SDO upload attempt %d, retrying...\n", err->toString(), i );
      size = max;
   }
   return err;
}

/***************************************************************************/
/**
Upload data using this SDO.  This internal function is used to make a single
upload attempt.

@param index The index of the object to be uploaded.
@param sub The sub-index of the object to be uploaded.
@param size On entry, this gives the maximum number of bytes of data to 
       be uploaded.  On successful return, it gives the actual number of bytes received.
@param data A character array which will store the uploaded data.
@return A valid CANopen error object.
*/
/***************************************************************************/
const Error *SDO::_Upload( int16 index, int16 sub, int32 &size, byte *data )
{
   const Error *err;
   uint8 buff[MAX_SDO_LEN];

   // Make sure the SDO has been initialized
   if( !node ) return &CanOpenError::NotInitialized;

   // Check for a reasonable size
   if( size <= 0 ) return &CanOpenError::BadParam;

   // Use a block upload if it makes sense to do so
   if( blkUpldOK && (size >= SDO_BLK_UPLD_THRESHOLD) )
      return BlockUpld( index, sub, size, data );

   MutexLocker ml( mutex );

   // send an "Initiate SDO upload" message.
   buff[0] = 0x40;

   // Copy the object multiplexor to the frame
   // and also make a local copy.
   buff[1] = mplex[0] = ByteCast(index);
   buff[2] = mplex[1] = ByteCast(index>>8);
   buff[3] = mplex[2] = ByteCast(sub);

   // Clear out the reserved bytes
   for( int i=4; i<8; i++ )
      buff[i] = 0;

   uint16 retLen = MAX_SDO_LEN;
   err = XmitSDO( buff, 8, &retLen, timeout );
   if( err ) return SendAbort( err );
   return UploadFinish( size, data, retLen, buff );
}

const Error *SDO::UploadFinish( int32 &size, byte *data, uint16 retLen, uint8 *buff )
{
   const Error *err;
   int32 remain = size;
   int toggle = 0;

   // Check for abort
   int scs = 7 & (buff[0]>>5);
   if( scs == 4 ) return getAbortRcvdErr( buff );

   // I expect an scs value of 2 (init upload response).
   if( scs != 2 )
      return SendAbort( &CanOpenError::SDO_BadMsgRcvd );

   // If the multiplexor was not right, abort the transfer
   if( (buff[1] != mplex[0]) || (buff[2] != mplex[1]) || (buff[3] != mplex[2]) )
      return SendAbort( &CanOpenError::SDO_BadMuxRcvd );

   int32 upldSize = -1;

   // Grab the size information passed with 
   // the message (if any is passed).
   if( buff[0] & 1 )
   {
      // Expedited transfer size info
      if( buff[0] & 2 )
         upldSize = 4 - (3&(buff[0]>>2));

      // Normal transfer size info
      else
         upldSize = bytes_to_int32( &buff[4] );
   }

   // If this is an expedited transfer, copy the data
   // (as much as I can handle) to my buffer.
   if( buff[0] & 2 )
   {
      int ct = (upldSize<0) ? 4 : upldSize;

      // Clip the size to the available memory
      if( ct > remain ) ct = remain;

      for( int i=0; i<ct; i++ )
         *data++ = buff[i+4];

      size = ct;
      return 0;
   }

   // For normal transfers I'll send an upload
   // request.  Note that I don't abort the 
   // transfer if the upload size doesn't match
   // my buffer size.  I'll just grab what ever
   // data I can handle.
   //
   // If the upload size was specified, and is 
   // less then my buffer size, I'll set my
   // remaining count equal to it.
   if( (upldSize >= 0) && (upldSize < remain) )
      remain = upldSize;
   size = 0;

   // For normal transfers, see if any additional data was
   // passed with the response (EtherCAT only).
   if( retLen > 8 )
   {
      int ct = retLen - 8;
      if( ct > remain ) ct = remain;
      for( int i=0; i<ct; i++ )
         *data++ = buff[8+i];
      remain -= ct;
      size += ct;
   }

   if( !remain ) 
      return 0;

   while( 1 )
   {
      int i;

      // Request more data
      buff[0] = toggle ? 0x70 : 0x60;
      for( i=1; i<8; i++ ) buff[i] = 0;

      toggle = !toggle;
      uint16 retLen = MAX_SDO_LEN;
      err = XmitSDO( buff, 8, &retLen, timeout );
      if( err ) return SendAbort( err );

      scs = 7 & (buff[0]>>5);

      // Check for abort
      if( scs == 4 ) return getAbortRcvdErr( buff );

      if( scs != 0 )
         return SendAbort( &CanOpenError::SDO_BadMsgRcvd );

      // Check the toggle bit
      int expected = toggle ? 0 : 0x10;
      if( (buff[0] & 0x10) != expected )
         return SendAbort( &SDO_Error::Togglebit );

      // If the number of bytes of data passed in this
      // message was specified, then decode it.
      // Otherwise, assume 7.
      int ct = 7 - (7&(buff[0]>>1));

      if( retLen > 8 )
         ct = retLen - 1;

      // If the number of bytes sent in the message is
      // greater then my buffer size, clip it.
      if( ct > remain ) ct = remain;

      // Copy the data
      for( i=0; i<ct; i++ )
         *data++ = buff[i+1];

      remain -= ct;
      size += ct;

      // If this was the last message then I'm done
      if( buff[0] & 1 )
         return 0;

      // Otherwise, if my buffer is full I'll
      // abort the transfer.  We don't need your
      // stinkin data!
      if( !remain )
         return SendAbort( SDO_ABORT_TOO_LONG, 0 );
   }
}

class BlockReceiver: public Receiver
{
public:
   BlockReceiver( uint8 *dat, int32 total, uint8 blkSz )
   {
      lastBlk = 0;
      remain = total;
      data = dat;
      count = 0;
      maxSeq = blkSz;
      done = 0;
   }

   int NewFrame( CanFrame &frame )
   {
      // Find the sequence number for this segment
      int seq = frame.data[0] & 0x7F;

      // If this is my expected sequence number, 
      // copy the data.
      if( seq == lastBlk+1 )
      {
         int ct = (remain<7) ? remain : 7;

         for( int i=1; i<=ct; i++ )
            *data++ = frame.data[i];

         remain -= ct;
         count += ct;
         lastBlk = seq;
      }

      // See if we are finished with the transfer
      if( (frame.data[0] & 0x80) && (lastBlk==seq) )
         done = 1;

      // See if we are finished with this block
      if( (frame.data[0] & 0x80) || (seq == maxSeq) )
      {
         for( int i=0; i<8; i++ )
            lastFrame[i] = frame.data[i];
         sem.Put();
      }
      return 1;
   }

   const Error *Wait( Timeout to )
   {
      const Error *err = sem.Get( to );

      if( err == &ThreadError::Timeout )
         err = &CanOpenError::SDO_Timeout;
      return err;
   }

   Semaphore sem;
   uint8 lastBlk;
   uint8 maxSeq;
   uint8 *data;
   int32 remain;
   int32 count;
   uint8 lastFrame[8];
   int done;
};

/***************************************************************************/
/**
Upload data using this SDO.  This function uses a block upload protocol
which makes sending large blocks of data more efficient.  The specified
object is upload from the CANopen node's object dictionary and stored in    
the array passed to this function.
 
@param index The index of the object to be uploaded.
@param sub The sub-index of the object to be uploaded.
@param size On entry, this should be the maximum number of bytes
       to upload, on successful return, this is the number of bytes
       actually received.
@param data A character array which will store the uploaded data.
@return A valid CANopen error object.
*/
/***************************************************************************/
const Error *SDO::BlockUpld( int16 index, int16 sub, int32 &size, byte *data )
{
   const Error *err;
   uint8 buff[MAX_SDO_LEN];

   // Make sure the SDO has been initialized
   if( !node ) return &CanOpenError::NotInitialized;

   // Check for a reasonable size
   if( size <= 0 ) return &CanOpenError::BadParam;

   MutexLocker ml( mutex );

   // send an "Initiate block upload" message.
   buff[0] = 0xA0;

   // Copy the object multiplexor to the frame
   // and also make a local copy.
   buff[1] = mplex[0] = ByteCast(index);
   buff[2] = mplex[1] = ByteCast(index>>8);
   buff[3] = mplex[2] = ByteCast(sub);

   // For now, I use 127 segments/block and allow
   // drop back to normal update if the number of
   // actual bytes is less then my threshold.
   buff[4] = 127;
   buff[5] = SDO_BLK_UPLD_THRESHOLD-1;

   // Clear out the reserved bytes
   buff[6] = 0;
   buff[7] = 0;

   uint16 retLen = MAX_SDO_LEN;
   err = XmitSDO( buff, 8, &retLen, timeout );
   if( err ) return SendAbort( err );

   // Check for abort
   int scs = 7 & (buff[0]>>5);
   if( scs == 4 ) return getAbortRcvdErr( buff );

   // Check for a fallback to normal upload method
   if( scs == 2 ) return UploadFinish( size, data, retLen, buff );

   // I expect an scs value of 6 (block upload response).
   if( scs != 6 ) return SendAbort( &CanOpenError::SDO_BadMsgRcvd );

   // Make sure the sub-code was valid
   if( buff[0] & 1 ) return SendAbort( &CanOpenError::SDO_BadMsgRcvd );

   // If the multiplexor was not right, abort the transfer
   if( (buff[1] != mplex[0]) || (buff[2] != mplex[1]) || (buff[3] != mplex[2]) )
      return SendAbort( &CanOpenError::SDO_BadMuxRcvd );

   int32 upldSize = -1;

   // Grab the size information passed with 
   // the message (if any is passed).
   if( buff[0] & 2 )
      upldSize = bytes_to_int32( &buff[4] );

   int32 remain = size;
   size = 0;

   // If the upload size was specified, and is 
   // less then my buffer size, I'll set my
   // remaining count equal to it.
   if( (upldSize > 0) && (upldSize < remain) )
      remain = upldSize;

   // Create a receiver which will copy data from the received blocks
   BlockReceiver blkRcvr( data, remain, 127 );
   {
      RefObjLocker<CanOpen> co( node->GetNetworkRef() );
      if( !co ) return &NodeError::NetworkUnavailable;
      err = co->EnableReceiver( 0x580+node->GetNodeID(), &blkRcvr );
      if( err ) return err;
   }

   // Start the block upload
   buff[0] = 0xA3;
   for( int i=1; i<8; i++ ) buff[i] = 0;

   while( 1 )
   {
      uint16 retLen = MAX_SDO_LEN;
      err = XmitSDO( buff, 8, &retLen, 0 );
      if( err ) return SendAbort( err );

      // If not done, wait for data
      if (!blkRcvr.done)
      {
         err = blkRcvr.Wait( timeout );
         if( err ) return SendAbort( err );
      }

      // Check for abort
      if( blkRcvr.lastFrame[0] == 0x80 ) return getAbortRcvdErr( blkRcvr.lastFrame );

      // Check for second time in loop after done
      if (blkRcvr.done >= 2 ) break;
      
      // See if we successfully received the last block
      // Enter loop once more to send the last response
      if( blkRcvr.done ) blkRcvr.done++;

      // Otherwise, request the next block of data
      buff[0] = 0xA2;
      buff[1] = blkRcvr.lastBlk;
      buff[2] = 127;

      blkRcvr.lastBlk = 0;
   }

   {
      RefObjLocker<CanOpen> co( node->GetNetworkRef() );
      if( !co ) return &NodeError::NetworkUnavailable;
      co->DisableReceiver( 0x580+node->GetNodeID() );
   }
  
   //Respond that block upload is done  
   buff[0] = 0xA1;
   buff[1] = 0;
   buff[2] = 0;
   retLen = MAX_SDO_LEN;
   err = XmitSDO( buff, 8, &retLen, timeout );
   if( err ) return SendAbort( err );

   size = blkRcvr.count;

   return 0;
}

/***************************************************************************/
/**
Download data using this SDO.  This function uses a block download protocol
which makes sending large blocks of data more efficient.  The data passed
to this function is downloaded to the object dictionary of the CANopen node

@param index The index of the object to be downloaded.
@param sub The sub-index of the object to be downloaded.
@param size The number of bytes of data to be downloaded
@param data A character array holding the data to be downloaded.
@return A valid CANopen error object.
*/
/***************************************************************************/
const Error *SDO::BlockDnld( int16 index, int16 sub, int32 size, byte *data )
{
   // This isn't supported yet, just return an error
   return &CanOpenError::NotSupported;
}

/***************************************************************************/
/**
Send an abort frame.
*/
/***************************************************************************/
const Error *SDO::SendAbort( const Error *err )
{
   switch( err->GetID() )
   {
      case CMLERR_CanOpenError_SDO_BadMsgRcvd: return SendAbort( SDO_ABORT_BAD_SCS,     err );
      case CMLERR_CanOpenError_SDO_BadMuxRcvd: return SendAbort( SDO_ABORT_GENERAL_ERR, err );
      case CMLERR_SDO_Error_Togglebit:         return SendAbort( SDO_ABORT_TOGGLEBIT,   err );
      default:                                 return SendAbort( SDO_ABORT_TIMEOUT,     err );
   }
}

const Error *SDO::SendAbort( int32 abortCode, const Error *err )
{
   uint8 data[MAX_SDO_LEN];
   data[0] = 0x80;
   data[1] = mplex[0];
   data[2] = mplex[1];
   data[3] = mplex[2];
   int32_to_bytes( abortCode, &data[4] );

   uint16 retLen = MAX_SDO_LEN;
   XmitSDO( data, 8, &retLen, 0 );
   return err;
}


const Error *SDO::XmitSDO( uint8 *buff, uint16 len, uint16 *retLen, Timeout timeout )
{
   RefObjLocker<Network> net( node->GetNetworkRef() );
   if( !net ) return &NodeError::NetworkUnavailable;
   return net->XmitSDO( node, buff, len, retLen, timeout );
}

/***************************************************************************/
/**
Translate the passed SDO abort code into an error message.
*/
/***************************************************************************/
const Error *SDO::getAbortRcvdErr( uint8 *buff )
{
   int32 code = bytes_to_int32( &buff[4] );

   switch( code )
   {
      case 0:                                return &SDO_Error::NoAbortCode;
      case SDO_ABORT_TOGGLEBIT:              return &SDO_Error::Togglebit;
      case SDO_ABORT_TIMEOUT:                return &SDO_Error::Timeout;
      case SDO_ABORT_BAD_SCS:                return &SDO_Error::Bad_scs;
      case SDO_ABORT_BLOCK_SIZE:             return &SDO_Error::Block_size;
      case SDO_ABORT_BLOCK_SEQ:              return &SDO_Error::Block_seq;
      case SDO_ABORT_BLOCK_CRC:              return &SDO_Error::Block_crc;
      case SDO_ABORT_MEMORY:                 return &SDO_Error::Memory;
      case SDO_ABORT_ACCESS:                 return &SDO_Error::Access;
      case SDO_ABORT_WRITEONLY:              return &SDO_Error::Writeonly;
      case SDO_ABORT_READONLY:               return &SDO_Error::Readonly;
      case SDO_ABORT_OBJ_MAP_ACTIVE:         return &SDO_Error::ObjMapActive;
      case SDO_ABORT_BAD_OBJECT:             return &SDO_Error::Bad_object;
      case SDO_ABORT_PDO_MAP:                return &SDO_Error::Pdo_map;
      case SDO_ABORT_PDO_LENGTH:             return &SDO_Error::Pdo_length;
      case SDO_ABORT_BAD_PARAM:              return &SDO_Error::Bad_param;
      case SDO_ABORT_INCOMPATIBLE:           return &SDO_Error::Incompatible;
      case SDO_ABORT_HARDWARE:               return &SDO_Error::Hardware;
      case SDO_ABORT_BAD_LENGTH:             return &SDO_Error::Bad_length;
      case SDO_ABORT_TOO_LONG:               return &SDO_Error::Too_long;
      case SDO_ABORT_TOO_SHORT:              return &SDO_Error::Too_short;
      case SDO_ABORT_SUBINDEX:               return &SDO_Error::Subindex;
      case SDO_ABORT_PARAM_RANGE:            return &SDO_Error::Param_range;
      case SDO_ABORT_PARAM_HIGH:             return &SDO_Error::Param_high;
      case SDO_ABORT_PARAM_LOW:              return &SDO_Error::Param_low;
      case SDO_ABORT_MIN_MAX:                return &SDO_Error::Min_max;
      case SDO_ABORT_GENERAL_ERR:            return &SDO_Error::General;
      case SDO_ABORT_TRANSFER:               return &SDO_Error::Transfer;      
      case SDO_ABORT_TRANSFER_LOCAL:         return &SDO_Error::Transfer_Local;
      case SDO_ABORT_TRANSFER_STATE:         return &SDO_Error::Transfer_State;
      case SDO_ABORT_OD_GEN_FAIL:            return &SDO_Error::OD_Gen_Fail;
      default:                               return &SDO_Error::Unknown;
   }
}

void SDO::SetMaxRetry( uint8 max )
{
   if( max < 1 ) max = 0;
   maxRetry = max;
}

