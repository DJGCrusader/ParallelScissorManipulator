/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2012 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/** \file
Implementation of the InputShaper class.
*/

#include "CML.h"

CML_NAMESPACE_USE();

/***************************************************************************/
/**
Default constructor for InputShaper object. Simply sets all impulses to 0
*/
/***************************************************************************/
InputShaper::InputShaper( void )
{
   // Setting the filter info field to 0xffff 
   // disables the filter
   int i;
   for( i=0; i<4; i++ )
      info[i] = 0xFFFF;

   // Initialize all impulse amplitudes and times to zero
   for( i=0; i<16; i++ )
   {
      inputShapeFilter[i] = 0;
   }
}

/***************************************************************************/
/**
  Return the amplitude for each impulse in the input shaping filter
*/
/***************************************************************************/
void InputShaper::setInputShapeFilter(float inputShaperData[])
{
   // Possibly pass in information about the size of the array?
   for(int i=0; i<16; i++)
      inputShapeFilter[i] = inputShaperData[i];
}

void InputShaper::getInputShapeFilter(float inputShaperData[])
{
   for(int i=0; i<16; i++)
      inputShaperData[i] = inputShapeFilter[i];
}

/***************************************************************************/
/**
  Load the filter coefficients from an array of integer values read from a 
  .ccx file.  

  The .ccx file holds an array of 20 long integer values for the input shaping
  filter.
  @param inputShaping The array of input shaping data read from the ccx file
  @param ct The number of coefficients (should be 20)
  @return An error pointer or NULL on success
  */
/***************************************************************************/
const Error *InputShaper::LoadFromCCX( int32 inputShaping[], int ct )
{
   // Check for correct formatting
   if(ct == 20)
   {
      // Get the first four segments, this contains the filter info
      for( int i=0; i<4; i++ )
         info[i] = inputShaping[i];

      // The remaining segements contain the filter data
      // The array consists of pairs of amplitude and time data
      // for each impulse in the shaping filter
      float *fptr = (float*)&inputShaping[4];
      setInputShapeFilter(fptr);

      if(fptr)
      {
         //delete fptr;
         fptr = 0;
      }

      return 0;
   }
   else
      return &AmpFileError::format;
}


const Error *InputShaper::Upld( SDO &sdo, int16 index, bool useFloat )
{
   // Maximum of 80 bytes of data can be uploaded for the
   // input shaping filter
   int32 ct = 80;
   byte data[80];

   for(int i=0; i<80; i++)
      data[i] = 0;

   // Upload 80 bytes of data
   const Error *err = sdo.Upload(index, 0, ct, data);
   if( err ) return err;

   arrayToImpulse( data );

   return 0;
}

const Error *InputShaper::Dnld( SDO &sdo, int16 index, bool useFloat )
{
   byte data[80];
   impulseToArray(  data );
   // Download the full 80 bytes of data
   return sdo.Download( index, 0, 80, data);
}

// For now, ct is always 80 bytes
void InputShaper::impulseToArray( byte data[] )
{
   int i, ct;
   float inShapFilt[16];
   int offset = 16;

   // Initialize the data to zero before downloading to amp.
   for( i=0; i<80; i++)
      data[i] = 0;

   getInputShapeFilter( inShapFilt );

   for( i=0; i<4; i++)
      int32_to_bytes( info[i], &data[4*i] );

   // Convert the floating points to bytes
   for( ct=0; ct<16; ct++ )
   {
      // 4 bytes for each IEEE floating point number
      float_to_bytes(inShapFilt[ct], &data[offset+(4*ct)]);
   }
}

void InputShaper::arrayToImpulse( byte data[] )
{
   int i;
   // The first four 32 bit ints give info about the filters
   for( i=0; i<4; i++ )
      info[i] = bytes_to_int32( &data[4*i] );

   float inShapFilt[16];
   // Offset the data array by the info already stored 
   int offset = 16;

   // Convert the uploaded bytes to an array of floats for impulse data
   for( i=0; i<16; i++)
      inShapFilt[i] = bytes_to_float( &data[offset+(4*i)] );

   setInputShapeFilter( inShapFilt );

}
