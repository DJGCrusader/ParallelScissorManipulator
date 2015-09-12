/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2010 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/***************************************************************************/
/** \file
This file contains the CopleyIO object methods used to upload / download 
structures containing groups of module parameters.
*/
/***************************************************************************/

#include "CML.h"

CML_NAMESPACE_USE();

/***************************************************************************/
/**
  Default constructor for a Copley I/O module.  
  */
/***************************************************************************/
CopleyIO::CopleyIO( void )
{
}

/***************************************************************************/
/**
  Construct a CopleyIO object and initialize it using default settings.
  @param net The Network object that this module is associated with.
  @param nodeID The node ID of the module on the network.
  */
/***************************************************************************/
CopleyIO::CopleyIO( Network &net, int16 nodeID ): IOModule( net, nodeID )
{
}

/***************************************************************************/
/**
  Construct a CopleyIO object and initialize it using custom settings.
  @param net The Network object that this module is associated with.
  @param nodeID The node ID of the module on the network.
  @param settings The settings to use when configuring the module
  */
/***************************************************************************/
CopleyIO::CopleyIO( Network &net, int16 nodeID, IOModuleSettings &settings ): IOModule( net, nodeID, settings )
{
}

/***************************************************************************/
/**
  Virtual destructor for the IOModule object.
  */
/***************************************************************************/
CopleyIO::~CopleyIO()
{
}

/***************************************************************************/
/**
  Initialize a Copley IO module using default settings.  This function associates 
  the object with the CANopen network it will be used on.

  @param net The Network object that this module is associated with.
  @param nodeID The node ID of the module on the network.
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::Init( Network &net, int16 nodeID )
{
   IOModuleSettings settings;
   return IOModule::Init( net, nodeID, settings );
}

/***************************************************************************/
/**
  Initialize an I/O module using custom settings.  This function associates the 
  object with the CANopen network it will be used on.

  @param net The Network object that this module is associated with.
  @param nodeID The node ID of the module on the network.
  @param settings The settings to use when configuring the module
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::Init( Network &net, int16 nodeID, IOModuleSettings &settings )
{
   return IOModule::Init( net, nodeID, settings );
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
                   I/O information
*/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/**
  Read the I/O Module information parameters from the drive.

  @param info A structure that will be filled with the I/O module info
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::GetIOInfo( CopleyIOInfo &info )
{
   int32 l;

   const Error *err = sdo.Upld32( CIOOBJID_INFO_SERIAL,    0, info.serial     );
   if( !err ) err = sdo.UpldString( CIOOBJID_INFO_MODEL,   0, l=COPLEYIO_MAX_STRING, info.model    );
   if( !err ) err = sdo.UpldString( CIOOBJID_INFO_MFGINFO, 0, l=COPLEYIO_MAX_STRING, info.mfgInfo  );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_HWTYPE,      0, info.hwType     );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_LOOPRATE,    0, info.loopRate   );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_FWVERSION,   0, info.fwVersion  );
   if( !err ) err = sdo.Upld32( CIOOBJID_INFO_BAUD,        0, info.baud       );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_MAXWORDS,    0, info.maxWords   );
   if( !err ) err = sdo.UpldString( CIOOBJID_INFO_NAME,    0, l=COPLEYIO_MAX_STRING, info.name     );
   if( !err ) err = sdo.UpldString( CIOOBJID_INFO_HOSTCFG, 0, l=COPLEYIO_MAX_STRING, info.hostCfg );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_NODECFG,     0, info.nodeCfg    );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_RATECFG,     0, info.rateCfg    );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_NODEID,      0, info.nodeID     );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_STATUS,      0, info.status     );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_RATE,        0, info.rate       );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_ANLGINT,     0, info.anlgInt    );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_ANLGINTENA,  0, info.anlgIntEna );
   if( !err ) err = sdo.Upld16( CIOOBJID_INFO_DIGIINTENA,  0, info.digiIntEna );
   if( !err ) err = sdo.Upld32( CIOOBJID_INFO_PWMPERIODA,  0, info.pwmPeriodA );
   if( !err ) err = sdo.Upld32( CIOOBJID_INFO_PWMPERIODB,  0, info.pwmPeriodB );

   return err;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
                          I/O digital
*/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/**
  Read the I/O Module digital parameters from the drive.

  @param digi A structure that will be filled with the digital I/O values
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::GetIODigi( CopleyIODigi &digi )
{
   const Error *err = 0;

   for( int i=0; i<COPLEYIO_DIO_BANKS; i++ )
   {
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_BANKMODE,  i+1, digi.bankMode[i]  );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_PULLUPMSK, i+1, digi.pullupMsk[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_TYPEMSK,   i+1, digi.typeMsk[i]   );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_FAULTMSK,  i+1, digi.faultMsk[i]  );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_INVMSK,    i+1, digi.invMsk[i]    );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_VALUEMSK,  i+1, digi.valueMsk[i]  );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_MODEMSK,   i+1, digi.modeMsk[i]   );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_RAWMSK,    i+1, digi.rawMsk[i]    );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_HILOMSK,   i+1, digi.hiLoMsk[i]   );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_LOHIMSK,   i+1, digi.loHiMsk[i]   );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_DEBOUNCE0, i+1, digi.debounce0[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_DEBOUNCE1, i+1, digi.debounce1[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_DEBOUNCE2, i+1, digi.debounce2[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_DEBOUNCE3, i+1, digi.debounce3[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_DEBOUNCE4, i+1, digi.debounce4[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_DEBOUNCE5, i+1, digi.debounce5[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_DEBOUNCE6, i+1, digi.debounce6[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_DIGI_DEBOUNCE7, i+1, digi.debounce7[i] );
   }

   return err;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
                           I/O analog
*/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/**
  Read the I/O Module analog parameters from the drive.

  @param anlg A structure that will be filled with the analog I/O values
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::GetIOAnlg( CopleyIOAnlg &anlg )
{
  const Error *err = 0;

   for( int i=0; i<COPLEYIO_NUM_AIN; i++ )
   {
      if( !err ) err = sdo.Upld16( CIOOBJID_ANLG_IRAW,      i+1, anlg.iRaw[i]      );
      if( !err ) err = sdo.Upld32( CIOOBJID_ANLG_ISCALED,   i+1, anlg.iScaled[i]   );
      if( !err ) err = sdo.Upld32( CIOOBJID_ANLG_IFACTOR,   i+1, anlg.iFactor[i]   );
      if( !err ) err = sdo.Upld32( CIOOBJID_ANLG_IOFFSET,   i+1, anlg.iOffset[i]   );
      if( !err ) err = sdo.Upld32( CIOOBJID_ANLG_IUPLIMIT,  i+1, anlg.iUpLimit[i]  );
      if( !err ) err = sdo.Upld32( CIOOBJID_ANLG_ILOLIMIT,  i+1, anlg.iLoLimit[i]  );
      if( !err ) err = sdo.Upld32( CIOOBJID_ANLG_IABSDELTA, i+1, anlg.iAbsDelta[i] );
      if( !err ) err = sdo.Upld32( CIOOBJID_ANLG_IPOSDELTA, i+1, anlg.iPosDelta[i] );
      if( !err ) err = sdo.Upld32( CIOOBJID_ANLG_INEGDELTA, i+1, anlg.iNegDelta[i] );
      if( !err ) err = sdo.Upld16( CIOOBJID_ANLG_IFLAGS,    i+1, anlg.iFlags[i]    );
      if( !err ) err = sdo.Upld16( CIOOBJID_ANLG_IMASK,     i+1, anlg.iMask[i]     );
   }

   return err;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
                                 I/O PWM
*/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/**
  Read the I/O Module PWM parameters from the drive.

  @param pwm A structure that will be filled with the PWM I/O values
  @param info A structure containing additional information about the module.
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::GetIOPWM( CopleyIOPWM &pwm, CopleyIOInfo &info )
{
   const Error *err = 0;
   uint8 ct;
   uint8 max = 2;
   uint8 opcode = 12;
   uint16 data[2];
   uint16 paramID = 0x51;

   /***********************NOTE************************/
   /* The following code was added to mask a firmware */
   /* bug that prevented access to CANID 0x3051 for   */
   /* versions < 23. This workaround allows access to */
   /* this feature through serial communication. See  */
   /* the Accenet Programmer's guide for more info.   */
   /* Firmware bug fixed 11/1/10, rev 24.             */
   /***************************************************/
   if( info.fwVersion <23 )
   {
      for( uint16 i=0; i<COPLEYIO_NUM_PWM; i++ )
      {
         data[0] = (i<<8) | (paramID);               //bank concatenated with ParamID
         data[1] = 0;
         ct = 1;

         if( !err ) err = SerialCmd( opcode, ct, max, data );
         pwm.oRaw[i] = ((uint32)data[1] << 16) | (uint32)data[0];
         if( !err ) err = sdo.Upld32( CIOOBJID_PWM_OSCALED,   i+1, pwm.oScaled[i]   );
         if( !err ) err = sdo.Upld32( CIOOBJID_PWM_OFACTOR,   i+1, pwm.oFactor[i]   );
         if( !err ) err = sdo.Upld32( CIOOBJID_PWM_OOFFSET,   i+1, pwm.oOffset[i]   );
      }
   }

   else
   {
      for( int i=0; i<COPLEYIO_NUM_PWM; i++ )
      {
         if( !err ) err = sdo.Upld16( CIOOBJID_PWM_ORAW,      i+1, pwm.oRaw[i]      );
         if( !err ) err = sdo.Upld32( CIOOBJID_PWM_OSCALED,   i+1, pwm.oScaled[i]   );
         if( !err ) err = sdo.Upld32( CIOOBJID_PWM_OFACTOR,   i+1, pwm.oFactor[i]   );
         if( !err ) err = sdo.Upld32( CIOOBJID_PWM_OOFFSET,   i+1, pwm.oOffset[i]   );
      }
   }

   return err;
}

/***************************************************************************/
/**
  Read the complete I/O configuration from the module and return it
  in the passed structure.  This structure holds every module parameter that
  can be stored to the module's internal flash memory.  The contents of the
  structure represent the complete I/O configuration.
  @param cfg The structure which will hold the uploaded configuration.
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::GetIOCfg( CopleyIOCfg &cfg )
{
   const Error *err = GetIOInfo( cfg.info );
   if( !err ) err = GetIODigi( cfg.digi );
   if( !err ) err = GetIOAnlg( cfg.anlg ); 
   if( !err ) err = GetIOPWM( cfg.pwm, cfg.info ); 

   return err;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
                             I/O information
*/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/**
  Write the I/O Module information parameters to the drive.

  @param info A structure that will be filled with the I/O module info
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::SetIOInfo( CopleyIOInfo &info )
{
   const Error *err = sdo.Download( CIOOBJID_INFO_NAME, 0, COPLEYIO_MAX_STRING-1, info.name );
   if( !err ) err = sdo.Download( CIOOBJID_INFO_HOSTCFG, 0, COPLEYIO_MAX_STRING-1, info.hostCfg );
   if( !err ) err = sdo.Dnld16( CIOOBJID_INFO_NODECFG,     0, info.nodeCfg    );
   if( !err ) err = sdo.Dnld16( CIOOBJID_INFO_RATECFG,     0, info.rateCfg    );
   if( !err ) err = sdo.Dnld16( CIOOBJID_INFO_ANLGINTENA,  0, info.anlgIntEna );
   if( !err ) err = sdo.Dnld16( CIOOBJID_INFO_DIGIINTENA,  0, info.digiIntEna );
   if( !err ) err = sdo.Dnld32( CIOOBJID_INFO_PWMPERIODA,  0, info.pwmPeriodA );
   if( !err ) err = sdo.Dnld32( CIOOBJID_INFO_PWMPERIODB,  0, info.pwmPeriodB );

   return err;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
                             I/O digital
*/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/**
  Write the I/O Module digital parameters to the drive.

  @param digi A structure that will be filled with the digital I/O values
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::SetIODigi( CopleyIODigi &digi )
{
   const Error *err = 0;

   for( int i=0; i<COPLEYIO_DIO_BANKS; i++ )
   {
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_BANKMODE,  i+1, digi.bankMode[i]  );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_PULLUPMSK, i+1, digi.pullupMsk[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_TYPEMSK,   i+1, digi.typeMsk[i]   );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_FAULTMSK,  i+1, digi.faultMsk[i]  );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_INVMSK,    i+1, digi.invMsk[i]    );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_VALUEMSK,  i+1, digi.valueMsk[i]  );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_MODEMSK,   i+1, digi.modeMsk[i]   );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_HILOMSK,   i+1, digi.hiLoMsk[i]   );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_LOHIMSK,   i+1, digi.loHiMsk[i]   );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_DEBOUNCE0, i+1, digi.debounce0[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_DEBOUNCE1, i+1, digi.debounce1[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_DEBOUNCE2, i+1, digi.debounce2[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_DEBOUNCE3, i+1, digi.debounce3[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_DEBOUNCE4, i+1, digi.debounce4[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_DEBOUNCE5, i+1, digi.debounce5[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_DEBOUNCE6, i+1, digi.debounce6[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_DIGI_DEBOUNCE7, i+1, digi.debounce7[i] );
   }

   return err;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
                            I/O analog
*/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/**
  Write the I/O Module analog parameters to the drive.

  @param anlg A structure that will be filled with the analog I/O values
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::SetIOAnlg( CopleyIOAnlg &anlg )
{
   const Error *err = 0;

   for( int i=0; i<COPLEYIO_NUM_AIN; i++ )
   {
      if( !err ) err = sdo.Dnld32( CIOOBJID_ANLG_IFACTOR,   i+1, anlg.iFactor[i]   );
      if( !err ) err = sdo.Dnld32( CIOOBJID_ANLG_IOFFSET,   i+1, anlg.iOffset[i]   );
      if( !err ) err = sdo.Dnld32( CIOOBJID_ANLG_IUPLIMIT,  i+1, anlg.iUpLimit[i]  );
      if( !err ) err = sdo.Dnld32( CIOOBJID_ANLG_ILOLIMIT,  i+1, anlg.iLoLimit[i]  );
      if( !err ) err = sdo.Dnld32( CIOOBJID_ANLG_IABSDELTA, i+1, anlg.iAbsDelta[i] );
      if( !err ) err = sdo.Dnld32( CIOOBJID_ANLG_IPOSDELTA, i+1, anlg.iPosDelta[i] );
      if( !err ) err = sdo.Dnld32( CIOOBJID_ANLG_INEGDELTA, i+1, anlg.iNegDelta[i] );
      if( !err ) err = sdo.Dnld16( CIOOBJID_ANLG_IMASK,     i+1, anlg.iMask[i]     );
   }

   return err;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
                              I/O PWM
*/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
/**
  Write the I/O Module PWM parameters to the drive.

  @param pwm A structure that will be filled with the PWM I/O values
  @param info A structure containing additional information about the module.
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::SetIOPWM( CopleyIOPWM &pwm, CopleyIOInfo &info )
{
   const Error *err = 0;
   uint8 ct;
   uint8 max = 3;
   uint8 opcode = 13;
   uint16 data[3];
   uint16 paramID = 0x51;

   /***********************NOTE************************/
   /* The following code was added to mask a firmware */
   /* bug that prevented access to CANID 0x3051 for   */
   /* versions < 23. This workaround allows access to */
   /* this feature through serial communication. See  */
   /* the Accenet Programmer's guide for more info.   */
   /* Firmware bug fixed 11/1/10, rev 24.             */
   /***************************************************/
   if( info.fwVersion < 23 )
   {
      for( uint16 i=0; i<COPLEYIO_NUM_PWM; i++ )
      {
         ct = 3;
         data[2] = (uint16)pwm.oScaled[i];          //lower half of oScaled
         data[1] = (uint16)(pwm.oScaled[i] >> 16);  //upper half of oScaled
         data[0] = (i<<8) | (paramID);              //bank concatenated with ParamID

         if( !err ) err = SerialCmd( opcode, ct, max, data );
         if( !err ) err = sdo.Dnld32( CIOOBJID_PWM_OFACTOR,   i+1, pwm.oFactor[i]   );
         if( !err ) err = sdo.Dnld32( CIOOBJID_PWM_OOFFSET,   i+1, pwm.oOffset[i]   );
      }
   }

   else
   {
      for( int i=0; i<COPLEYIO_NUM_PWM; i++ )
      {
         if( !err ) err = sdo.Dnld32( CIOOBJID_PWM_OSCALED,   i+1, pwm.oScaled[i]   );
         if( !err ) err = sdo.Dnld32( CIOOBJID_PWM_OFACTOR,   i+1, pwm.oFactor[i]   );
         if( !err ) err = sdo.Dnld32( CIOOBJID_PWM_OOFFSET,   i+1, pwm.oOffset[i]   );
      }
   }

   return err;
}

/***************************************************************************/
/**
  Write the complete I/O configuration from the module and return it
  in the passed structure.  This structure holds every I/O parameter that
  can be stored to the module's internal flash memory.  The contents of the
  structure represent the complete I/O configuration.
  @param cfg The structure which will hold the uploaded configuration.
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::SetIOConfig( CopleyIOCfg &cfg )
{
   const Error *err = SetIOInfo( cfg.info );
   if( !err ) err = SetIODigi( cfg.digi );
   if( !err ) err = SetIOAnlg( cfg.anlg ); 
   if( !err ) err = SetIOPWM( cfg.pwm, cfg.info ); 

   return err;
}

/***************************************************************************/
/**
  Save all I/O parameters to internal flash memory.  Flash memory is a 
  type of non-volatile RAM which allows module parameters to be saved 
  between power cycles.  When this function is called, any module parameters
  that may be stored to flash will be copied from their working (RAM) locations
  to the stored (flash) locations.

  For a list of those I/O parameters which may be saved to flash memory,
  see the IOConfig structure.  Every member of that structure represents an
  io module parameter that may be saved to flash.

  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::SaveIOConfig( void )
{
   return sdo.Dnld32( 0x1010, 1, 0x65766173 );
}

/***************************************************************************/
/**
  Upload the passed io module configuration to the module's working memory,
  and then copy that working memory to flash.

  @param cfg The structure which holds the new configuration.
  @return A pointer to an error object, or NULL on success
  */
/***************************************************************************/
const Error *CopleyIO::SaveIOConfig( CopleyIOCfg &cfg )
{
   const Error *err = SetIOConfig( cfg );
   if( !err ) err = SaveIOConfig();
   return err;
}

/***************************************************************************/
/**
Send a serial port message to a Copley device over the CANopen network.

Copley devices use serial ports for some basic configuration purposes.  Most
of the functions available over the serial interface are also available in 
the CANopen object dictionary, but not everything.

This function allows a native serial port command to be sent over the CANopen
network.  It allows any remaining features of the device to be accessed when
only a CANopen connection is available.

@param opcode The command code to be sent to the amplifier.
@param ct     The number of 16-bit words of data to be sent to the amplifier.  On
              return, this parameter will hold the number of 16-bit words of response
              data passed back from the amplifier.
@param max    The maximum number of words of response data that the data array can 
              hold.
@param data   An array of data to be passed to the node with the command.  On return,
              any response data (up to max words) will be passed here.  
              If this parameter is not specified, then no data will be passed or returned
              regardless of the values passed in max and ct.

@return       An error object, or null on success.
*/
/***************************************************************************/
const Error *CopleyIO::SerialCmd( uint8 opcode, uint8 &ct, uint8 max, uint16 *data )
{
   #define MAX_MSG_BYTES 200

   if( !data ) max = ct = 0;

   if( ct > MAX_MSG_BYTES/2 )
      return &CopleyNodeError::SerialMsgLen;

   uint8 buff[ MAX_MSG_BYTES + 1 ];
   buff[0] = opcode;

   int i;
   for( i=0; i<ct; i++ )
   {
      buff[2*i+1] = (uint8)data[i];
      buff[2*i+2] = (uint8)(data[i]>>8);
   }

   const CML::Error *err = sdo.Download( 0x2000, 0, 2*ct+1, buff );
   if( err ) return err;

   int32 len = MAX_MSG_BYTES+1;
   err = sdo.Upload( 0x2000, 0, len, (uint8 *)buff );
   if( err ) return err;

   lastSerialError = buff[0];
   if( lastSerialError )
      return &CopleyNodeError::SerialError;

   ct = (len-1)/2;
   if( ct > max ) ct = max;

   for( i=0; i<ct; i++ )
      data[i] = bytes_to_uint16( &buff[2*i+1] );

   return 0;
}

