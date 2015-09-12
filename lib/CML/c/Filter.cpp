/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/** \file
Implementation of the Filter class.
*/

#include "CML.h"

CML_NAMESPACE_USE();

/***************************************************************************/
/**
Default constructor for filter object.  Simply sets all coefficients to zero.
*/
/***************************************************************************/
Filter::Filter( void )
{
   // Setting the filter info field to 0xffff 
   // disables the filter
   int i;
   for( i=0; i<4; i++ )
      info[i] = 0xFFFF;

   // Initialize all filter coefficients to zero
   for( i=0; i<6; i++ ) icoef[i] = 0;
   for( i=0; i<5; i++ ) fcoef[i] = 0.0f;

   asFloat = false;
}

/***************************************************************************/
/**
  Return the filter coefficients in floating point format as used by FPGA based amplifiers.
*/
/***************************************************************************/
void Filter::getFloatCoef( float &a1, float &a2, float &b0, float &b1, float &b2 )
{
   if( asFloat )
   {
      a1 = fcoef[0];
      a2 = fcoef[1];
      b0 = fcoef[2];
      b1 = fcoef[3];
      b2 = fcoef[4];
      return;
   }

   float scale = icoef[5] / 134217728.0f;

   a1 = icoef[0] * scale;
   a2 = icoef[1] * scale;
   b0 = icoef[2] * scale;
   b1 = icoef[3] * scale;
   b2 = icoef[4] * scale;
}

static inline float fabsf( float f )
{
   return (f<0) ? -f : f;
}

static inline int16 round( float f )
{
   int32 x = (f>0) ? (int32)(f+0.5) : (int32)(f-0.5);
   if( x > 32767 ) return 32767;
   if( x < -32768 ) return -32768;
   return x;
}

/***************************************************************************/
/**
  Return the filter coefficients in integer format as used by DSP based amplifiers.
*/
/***************************************************************************/
void Filter::getIntCoef( int16 &a1, int16 &a2, int16 &b0, int16 &b1, int16 &b2, int16 &k )
{
   if( !asFloat )
   {
      a1 = icoef[0];
      a2 = icoef[1];
      b0 = icoef[2];
      b1 = icoef[3];
      b2 = icoef[4];
      k  = icoef[5];
      return;
   }

   // Find the maximum absolute value of any of the coefficients
   float max = fabsf( fcoef[0] );
   int i;
   for( i=1; i<5; i++ )
   {
      float a = fabsf( fcoef[i] );
      if( a > max ) max = a;
   }

   // Find a scaling factor
   int32 X = (int32)(max * 4096.0f + 0.99);
   if( X > 32767 ) X = 32767;
   k = X;

   // Adjust the coefficients
   int16 tmp[5];
   for( i=0; i<5; i++ )
      tmp[i] = round( fcoef[i] * 134217728.0f / k );

   a1 = tmp[0];
   a2 = tmp[1];
   b0 = tmp[2];
   b1 = tmp[3];
   b2 = tmp[4];

   return;
}

void Filter::setIntCoef( int16 a1, int16 a2, int16 b0, int16 b1, int16 b2, int16 k )
{
   icoef[0] = a1;
   icoef[1] = a2;
   icoef[2] = b0;
   icoef[3] = b1;
   icoef[4] = b2;
   icoef[5] = k;
   asFloat = false;
}

void Filter::setFloatCoef( float a1, float a2, float b0, float b1, float b2 )
{
   fcoef[0] = a1;
   fcoef[1] = a2;
   fcoef[2] = b0;
   fcoef[3] = b1;
   fcoef[4] = b2;
   asFloat = true;
}

// New functions that handle the packing/unpacking of the filter coefficient arrays
void Filter::fcoefToArray( int &ct, byte data[])
{
   //data[28];
   ct = 0;
   // The first 6 bytes contain general information about the filter.
   for( int i=0; i<3; i++ )
   {
      data[ct++] = ByteCast(info[i]);
      data[ct++] = ByteCast(info[i]>>8);
   }

   // Floating point filters have an additional 2 bytes of info
   data[ct++] = ByteCast(info[3]);
   data[ct++] = ByteCast(info[3]>>8);

   // Find the floating point coefficients
   float a1, a2, b0, b1, b2;
   getFloatCoef( a1, a2, b0, b1, b2 );

   // Convert to bytes in the order expected by the amp
   float_to_bytes( a1, &data[ct] ); ct+= 4;
   float_to_bytes( a2, &data[ct] ); ct+= 4;
   float_to_bytes( b0, &data[ct] ); ct+= 4;
   float_to_bytes( b1, &data[ct] ); ct+= 4;
   float_to_bytes( b2, &data[ct] ); ct+= 4;
}

void Filter::icoefToArray( int &ct, byte data[] )
{
   //data[28];
   ct = 0;
   // The first 6 bytes contain general information about the filter.
   for( int i=0; i<3; i++ )
   {
      data[ct++] = ByteCast(info[i]);
      data[ct++] = ByteCast(info[i]>>8);
   }

   // Find the integer coefficients
   int16 a1, a2, b0, b1, b2, k;
   getIntCoef( a1, a2, b0, b1, b2, k );

   // Convert to bytes in the order expected by the amp
   int16_to_bytes( b2, &data[ct] ); ct += 2;
   int16_to_bytes( b1, &data[ct] ); ct += 2;
   int16_to_bytes( b0, &data[ct] ); ct += 2;
   int16_to_bytes( a2, &data[ct] ); ct += 2;
   int16_to_bytes( a1, &data[ct] ); ct += 2;
   int16_to_bytes( k,  &data[ct] ); ct += 2;
}

void Filter::arrayTofcoef( int ct, byte data[] )
{
   // The first 6 bytes give info about the filter
   for( int i=0; i<3; i++ )
      info[i] = bytes_to_uint16( &data[2*i] );

   // Floating point filters have two additional info bytes
   info[3] = bytes_to_uint16( &data[6] );

   float a1 = bytes_to_float( &data[8] );
   float a2 = bytes_to_float( &data[12] );
   float b0 = bytes_to_float( &data[16] );
   float b1 = bytes_to_float( &data[20] );
   float b2 = bytes_to_float( &data[24] );
   setFloatCoef( a1, a2, b0, b1, b2 );
}

void Filter::arrayToicoef( int ct, byte data[] )
{
   // The first 6 bytes give info about the filter
   for( int i=0; i<3; i++ )
      info[i] = bytes_to_uint16( &data[2*i] );

   int16 b2 = bytes_to_int16( &data[6] );
   int16 b1 = bytes_to_int16( &data[8] );
   int16 b0 = bytes_to_int16( &data[10] );
   int16 a2 = bytes_to_int16( &data[12] );
   int16 a1 = bytes_to_int16( &data[14] );
   int16 k  = bytes_to_int16( &data[16] );
   setIntCoef( a1, a2, b0, b1, b2, k );
}

/***************************************************************************/
/**
  Load the filter coefficients from an array of integer values read from a 
  .ccx file.  

  For DSP based amps the ccx file contains an array of 9 short integers holding
  the coefficient data.  For FPGA based amps the ccx file holds an array of 7
  long integer values.
  @param coef The array of coefficient data read from the ccx file
  @param ct The number of coefficients (should be either 7 or 9)
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *Filter::LoadFromCCX( int32 coef[], int ct )
{
   // Load short coefficients
   if( ct == 9 )
   {
      for( int i=0; i<3; i++ )
         info[i] = coef[i];
      // Set the integer coefs. Note the order of the array elements corresponds
      // to how this function expects to see the coefs...a1, a2, b0, b1, b2, k
      setIntCoef( coef[7], coef[6], coef[5], coef[4], coef[3], coef[8] );
      return 0;
   }

   else if( ct == 7 )
   {
      info[0] = coef[0];
      info[1] = coef[0]>>16;
      info[2] = coef[1];
      info[3] = coef[1]>>16;

      float *fptr = (float*)&coef[2];
      setFloatCoef( fptr[0], fptr[1], fptr[2], fptr[3], fptr[4] );
      return 0;
   }

   else
      return &AmpFileError::format;

}

const Error *Filter::Upld( SDO &sdo, int16 index, bool useFloat )
{
   int32 ct = 28;
   byte data[28];

   // Initialize the data array to zero.  If the amp doesn't send
   // enough data, we'll just treat the unsent bytes as zeros.
   
   int i;
   for(i=0; i<28; i++ ) data[i] = 0;

   const Error *err = sdo.Upload( index, 0, ct, data );
   if( err ) return err;

   if( useFloat )
   {
      arrayTofcoef(ct, data);
   }
   else
   {
      arrayToicoef(ct, data);
   }
   return 0;
}

const Error *Filter::Dnld( SDO &sdo, int16 index, bool useFloat )
{
   byte data[28];
   int ct = 0;

   if( useFloat )
   {
      fcoefToArray(ct, data);
   }

   else
   {
      icoefToArray(ct, data);
   }

   return sdo.Download( index, 0, ct, data );
}

/***************************************************************************/
/**
Load the filter coefficient structure given an array of 9 16-bit words of data.
This function is useful for loading the filter based on data read from a 
CME-2 amplifier data file.
*/
/***************************************************************************/
/*
void Filter::fromWords( int16 data[] )
{
}
*/

#if 0
static const Error *StrToFilter( char *str, Filter &flt )
{
   char *seg[10];

   if( SplitLine( str, seg, 10, ':' ) != 9 )
      return &AmpFileError::format;


   const Error *err = 0;
   int16 coef[9];
   for( int i=0; i<9; i++ )
   {
      if( !err ) 
         err = StrToInt16( seg[i], coef[i] );
   }
   
   // FIXME
   //flt.fromWords( coef );
   return err;
}

#endif
