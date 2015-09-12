/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2010 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/***************************************************************************
 File: IOFile.cpp                                                        
                                                                         
 This file contains code used to read a CME-2 .cci I/O file.             
                                                                     
***************************************************************************/


#include "CML.h"

#ifdef CML_FILE_ACCESS_OK
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#endif


CML_NAMESPACE_USE();

CML_NEW_ERROR( IOFileError, bankInvalid,   "I/O bank is invalid" );

/***************************************************************************/
/**
  Load the specified io module data file.  This function presently supports
  loading *.cci files created by the CME-2 program, version 1 and later.
  @param name The name (and optionally path) of the file to load
  @param line If not NULL, the last line number read from the file is returned
  here.  This is useful for finding file format errors.
  @return A pointer to an error object, or NULL on success.
  */
/***************************************************************************/
const Error *CopleyIO::LoadFromFile( const char *name, int &line )
{
#define MAX_LINE_LEN  200
#define MAX_LINE_SEGS 4

   line = 0;
#ifndef CML_FILE_ACCESS_OK
   return &AmpFileError::noFileAccess;
#else
   CopleyIOCfg cfg;

   // Load the configuration structure with the current I/O module
   // configuration data.  This ensures that any parameters not 
   // specified in the file will remain unchanged.
   const Error *err = GetIOCfg( cfg );
   if( err ) return err;

   // Open the file and read each parameter into my configuration
   // structure.
   FILE *fp;

   fp = fopen( name, "rt" );
   if( !fp )
      return &AmpFileError::fileOpen;

   char buff[MAX_LINE_LEN];
   char *seg[MAX_LINE_SEGS];
   int ct;
   int numOfSegs = 4;
   int segIndex = 3;

   int16 fileRev;

   // Read file version number
   line++;
   ReadLine( fp, buff, MAX_LINE_LEN );
   SplitLine( buff, seg, MAX_LINE_SEGS );

   //Third parameter indicates base of returned value (10)
   err = StrToInt16( seg[0], fileRev, 10 ); 
   if( err ) return err;

   if( fileRev > 1 ) 
      return &AmpFileError::format;

   // Read all parameters
   while( !feof(fp) && !err )
   {
      int16 param;
      int16 bank;

      line++;
      ReadLine( fp, buff, MAX_LINE_LEN );

      ct = SplitLine( buff, seg, MAX_LINE_SEGS );
      if( ct == 0 )
         continue;

      if( ct != numOfSegs )
         err = &AmpFileError::format;
      else
      {
         err = StrToInt16( seg[1], bank, 16 );
         if( err ) break;

         if( bank > 12 ) 
            return &IOFileError::bankInvalid;

         err = StrToInt16( seg[0], param, 16 );
      }

      if( err ) break;

      switch( param )
      {
         /// Begin general parameters
         case 0x000: err = StrToUInt32( seg[segIndex], cfg.info.serial ); break;
         case 0x001: strncpy( cfg.info.model, seg[segIndex], COPLEY_MAX_STRING ); break;
         case 0x002: strncpy( cfg.info.mfgInfo, seg[segIndex], COPLEY_MAX_STRING ); break;
         case 0x003: err = StrToUInt16( seg[segIndex], cfg.info.hwType ); break;
         case 0x004: err = StrToUInt16( seg[segIndex], cfg.info.loopRate ); break;
         case 0x010: err = StrToUInt16( seg[segIndex], cfg.info.fwVersion ); break;
         case 0x011: err = StrToUInt32( seg[segIndex], cfg.info.baud ); break;
         case 0x012: err = StrToUInt16( seg[segIndex], cfg.info.maxWords ); break;
         case 0x013: strncpy( cfg.info.name, seg[segIndex], COPLEY_MAX_STRING ); break;
         case 0x014: err = StrToHostCfg( seg[segIndex], cfg.info.hostCfg ); break;
         case 0x015: err = StrToInt16( seg[segIndex], cfg.info.nodeCfg ); break;
         case 0x016: err = StrToUInt16( seg[segIndex], cfg.info.rateCfg ); break;
         case 0x017: err = StrToUInt16( seg[segIndex], cfg.info.nodeID ); break;
         case 0x018: err = StrToUInt16( seg[segIndex], cfg.info.status ); break;
         case 0x019: err = StrToUInt16( seg[segIndex], cfg.info.rate ); break;
         case 0x01a: err = StrToUInt16( seg[segIndex], cfg.info.anlgInt ); break;
         case 0x01b: err = StrToUInt16( seg[segIndex], cfg.info.anlgIntEna ); break;
         case 0x01c: err = StrToUInt16( seg[segIndex], cfg.info.digiIntEna ); break;
         case 0x01e: err = StrToUInt32( seg[segIndex], cfg.info.pwmPeriodA ); break;
         case 0x01f: err = StrToUInt32( seg[segIndex], cfg.info.pwmPeriodB ); break;

                     /// Begin digital I/O parameters
         case 0x020: err = StrToUInt16( seg[segIndex], cfg.digi.bankMode[bank] ); break;
         case 0x021: err = StrToUInt16( seg[segIndex], cfg.digi.pullupMsk[bank] ); break;
         case 0x022: err = StrToUInt16( seg[segIndex], cfg.digi.typeMsk[bank] ); break;
         case 0x023: err = StrToUInt16( seg[segIndex], cfg.digi.faultMsk[bank] ); break;
         case 0x024: err = StrToUInt16( seg[segIndex], cfg.digi.invMsk[bank] ); break;
         case 0x025: err = StrToUInt16( seg[segIndex], cfg.digi.valueMsk[bank] ); break;
         case 0x026: err = StrToUInt16( seg[segIndex], cfg.digi.modeMsk[bank] ); break;
         case 0x027: err = StrToUInt16( seg[segIndex], cfg.digi.rawMsk[bank] ); break;
         case 0x028: err = StrToUInt16( seg[segIndex], cfg.digi.hiLoMsk[bank] ); break;
         case 0x029: err = StrToUInt16( seg[segIndex], cfg.digi.loHiMsk[bank] ); break;
         case 0x030: err = StrToUInt16( seg[segIndex], cfg.digi.debounce0[bank] ); break;
         case 0x031: err = StrToUInt16( seg[segIndex], cfg.digi.debounce1[bank] ); break;
         case 0x032: err = StrToUInt16( seg[segIndex], cfg.digi.debounce2[bank] ); break;
         case 0x033: err = StrToUInt16( seg[segIndex], cfg.digi.debounce3[bank] ); break;
         case 0x034: err = StrToUInt16( seg[segIndex], cfg.digi.debounce4[bank] ); break;
         case 0x035: err = StrToUInt16( seg[segIndex], cfg.digi.debounce5[bank] ); break;
         case 0x036: err = StrToUInt16( seg[segIndex], cfg.digi.debounce6[bank] ); break;
         case 0x037: err = StrToUInt16( seg[segIndex], cfg.digi.debounce7[bank] ); break;

                     /// Begin analog input parameters
         case 0x040: err = StrToUInt16( seg[segIndex], cfg.anlg.iRaw[bank] ); break;
         case 0x041: err = StrToUInt32( seg[segIndex], cfg.anlg.iScaled[bank] ); break;
         case 0x042: err = StrToUInt32( seg[segIndex], cfg.anlg.iFactor[bank] ); break;
         case 0x043: err = StrToUInt32( seg[segIndex], cfg.anlg.iOffset[bank] ); break;
         case 0x044: err = StrToUInt32( seg[segIndex], cfg.anlg.iUpLimit[bank] ); break;
         case 0x045: err = StrToUInt32( seg[segIndex], cfg.anlg.iLoLimit[bank] ); break;
         case 0x046: err = StrToUInt32( seg[segIndex], cfg.anlg.iAbsDelta[bank] ); break;
         case 0x047: err = StrToUInt32( seg[segIndex], cfg.anlg.iPosDelta[bank] ); break;
         case 0x048: err = StrToUInt32( seg[segIndex], cfg.anlg.iNegDelta[bank] ); break;
         case 0x049: err = StrToUInt16( seg[segIndex], cfg.anlg.iFlags[bank] ); break;
         case 0x04a: err = StrToUInt16( seg[segIndex], cfg.anlg.iMask[bank] ); break;

                     /// Begin PWM output parameters
         case 0x050: err = StrToUInt16( seg[segIndex], cfg.pwm.oRaw[bank] ); break;
         case 0x051: err = StrToUInt32( seg[segIndex], cfg.pwm.oScaled[bank] ); break;
         case 0x052: err = StrToUInt32( seg[segIndex], cfg.pwm.oFactor[bank] ); break;
         case 0x053: err = StrToInt32( seg[segIndex], cfg.pwm.oOffset[bank] ); break;

         default:
            cml.Debug( "Unknown paramaeter in CCX file: 0x%02x\n", param );
            break;
      }
   }

   fclose(fp);

   if( err ) return err;

   // The file was read in successfully.  Now, upload the configuration
   // structure to the I/O module.*/
   return SetIOConfig( cfg );
#endif
}
