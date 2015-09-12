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
  This file contains code used to parse CME-2 type files.
  */
/***************************************************************************/

#include "CML.h"

#ifdef CML_FILE_ACCESS_OK
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#endif


CML_NAMESPACE_START()

#ifdef CML_FILE_ACCESS_OK
/***************************************************************************/
/**
  Read a string from the passed file.  The string is read until the EOF or end
  of line is encountered.  If the line is too long to fit in the passed buffer,
  the line will be clipped, but the fill will be read until the EOL and any 
  extra data will be lost.
  @param ptr The file pointer of an open file
  @param buff The buffer where the data will be returned
  @param max The size of the buffer.
  @return A pointer to an error object, or NULL on success.
  */
/***************************************************************************/
const Error *ReadLine( void *ptr, char *buff, int max )
{
   FILE *fp = (FILE *)ptr;

   for( int i=0; ; )
   {
      int c = getc(fp);

      if( c == EOF || c == '\n' )
      {
         buff[i] = 0;
         return 0;
      }

      if( i < (max-1) )
         buff[i++] = c;
   }
}


/***************************************************************************/
/**
  Split the passed string into segments delimited by a character.
  This also strips white space at the start and end of each segment.
  @param buff The string to split.  This should end with a terminating zero.
  @param seg An array of character pointers which will point to the split
  line segments on return
  @param max The maximum number of segments to split the line into.
  @param delim The delimiter (comma by default)
  @return The number of segments the line was split into
  */
/***************************************************************************/
int SplitLine( char *buff, char **seg, int max, char delim )
{
   int i;
   for( i=0; i<max && *buff; i++ )
   {
      // Skip leading white space
      while( isspace(*buff) )
         buff++;

      if( i==0 && *buff==0 )
         break;

      // Grab start of next segment
      seg[i] = buff;

      // Find end
      while( *buff && *buff != delim )
         buff++;

      // Strip trailing white space
      for( char *ptr = buff-1; ptr >= seg[i] && isspace(*ptr); *ptr-- = 0 ){}

      // End segment
      if( *buff == delim )
         *buff++ = 0;
   }
   return i;
}

/***************************************************************************/
/**
  Convert an ASCII string into a 32-bit integer value.
  @param str The string to convert
  @param i The integer value will be returned here
  @param base The base to use during the conversion
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *StrToInt32( char *str, int32 &i, int base )
{
   char *end;
   i = (int32)strtol( str, &end, base );

   if( *end )
      return &AmpFileError::format;
   return 0;
}

/***************************************************************************/
/**
  Convert an ASCII string into a 32-bit unsigned integer value.
  @param str The string to convert
  @param i The integer value will be returned here
  @param base The base to use during the conversion
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *StrToUInt32( char *str, uint32 &i, int base )
{
   char *end;
   i = (int32)strtoul( str, &end, base );

   if( *end )
      return &AmpFileError::format;
   return 0;
}

/***************************************************************************/
/**
  Convert an ASCII string into a 16-bit integer value.
  @param str The string to convert
  @param i The integer value will be returned here
  @param base The base to use during the conversion
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *StrToInt16( char *str, int16 &i, int base )
{
   int32 l;
   const Error *err = StrToInt32( str, l, base );
   if( err ) return err;

   if( (l < -32768) || (l>32767) )
      return &AmpFileError::range;

   i = (int16)l;
   return 0;
}

/***************************************************************************/
/**
  Convert an ASCII string into a 16-bit unsigned integer value.
  @param str The string to convert
  @param i The integer value will be returned here
  @param base The base to use during the conversion
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *StrToUInt16( char *str, uint16 &i, int base )
{
   uint32 l;
   const Error *err = StrToUInt32( str, l, base );
   if( err ) return err;

   if( (l>65535) )
      return &AmpFileError::range;

   i = (uint16)l;
   return 0;
}

/***************************************************************************/
/**
  Convert an ASCII string (as read from a .ccx file) into an output pin
  configuration & mask value.
  @param str The string to decode
  @param cfg The configuration will be returned here
  @param mask1 The first output pin mask value will be returned here.
  @param mask2 The second output pin mask value will be returned here.
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *StrToOutCfg( char *str, OUTPUT_PIN_CONFIG &cfg, uint32 &mask1, uint32 &mask2 )
{
   const Error *err = 0;
   char *seg[4];
   int16 c;

   // The second mask is optional.  If it's not in the 
   // file then it defaults to zero.
   mask2 = 0;

   switch( SplitLine( str, seg, 4, ':' ) )
   {
      case 3:
         err = StrToUInt32( seg[2], mask2, 16 );

      case 2:
         if( !err ) err = StrToInt16( seg[0], c, 16 );
         if( !err ) err = StrToUInt32( seg[1], mask1, 16 );
         cfg = (OUTPUT_PIN_CONFIG)c;
         break;

      default:
         return &AmpFileError::format;
   }

   return err;
}
/***************************************************************************/
/**
  Convert an ASCII string (as read from a .ccx file) into a set of filter
  coefficients.
  @param str The string to decode
  @param flt The filter structure to be filled 
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *StrToFilter( char *str, Filter &flt )
{
   char *seg[10];

   int ct = SplitLine( str, seg, 10, ':' );

   if( (ct!=7) && (ct!=9) ) 
      return &AmpFileError::format;

   int32 coef[9];

   const Error *err = 0;
   for( int i=0; i<ct; i++ )
   {
      if( !err ) 
         err = StrToInt32( seg[i], coef[i] );
   }

   if( !err )
      err = flt.LoadFromCCX( coef, ct );

   return err;
}

/***************************************************************************/
/**
  Convert an ASCII string (as read from a .ccx file) into a set of input
  shaper impulses.
  @param str The string to decode
  @param inShape The filter structure to be filled 
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *StrToInputShaper( char *str, InputShaper &inShape )
{
   char *seg[20];
   int32 impulse[20];

   int ct = SplitLine( str, seg, 20, ':' );
   if( ct != 20 )
      return &AmpFileError::format;

   const Error *err = 0;

   // Copy all 20 paramters from the string and convert them to integers
   for( int i=0; i<ct; i++ )
   {
      err = StrToInt32( seg[i], impulse[i] );
   }

   if( !err )
      err = inShape.LoadFromCCX( impulse, ct ); 

   return err;
}

const Error *StrToHostCfg( char *str, char *hostCfg )
{
   char *seg[20];

   if( SplitLine( str, seg, 20, ':' ) != 20 )
      return &AmpFileError::format;

   const Error *err = 0;
   for( int i=0; i<20; i++ )
   {
      int16 c;
      err = StrToInt16( seg[i], c );
      if( err ) return err;

      hostCfg[2*i  ] = (char)(c>>8);
      hostCfg[2*i+1] = (char)(c);
   }

   return 0;
}

#endif

CML_NAMESPACE_END()
