/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2012 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/** \file

This file defines the InputShaper object.

The InputShaper represents a series of impulse functions
convolved with an input function.

*/

#ifndef _DEF_INC_INPUTSHAPER
#define _DEF_INC_INPUTSHAPER

#include "CML_Settings.h"
#include "CML_SDO.h"
#include "CML_Utils.h"

CML_NAMESPACE_START()

/***************************************************************************/
/**
Generic input shaper structure.  This structure holds the amplitudes and times
of the impulse functions used to create the input shaper, as well as the info
used by CME2 to indentify the filter type and settings.
*/
/***************************************************************************/
class InputShaper
{
   /// These words hold information about the filter.  They are 
   /// presently reserved for use by the CME program.
   int32 info[4];

   // Input shaping filters take in 8 pairs of amplitude/time pairs
   // for the impulse functions
   float inputShapeFilter[16];

public:
   InputShaper(void);

   const Error *LoadFromCCX( int32 inputShaping[], int ct );
   const Error *Dnld( SDO &sdo, int16 index, bool useFloat );
   const Error *Upld( SDO &sdo, int16 index, bool useFloat );
   void impulseToArray( byte data[] );
   void arrayToImpulse( byte data[] );
   void getInputShapeFilter( float inputShaperData[] );
   void setInputShapeFilter( float inputShaperData[] );
};

CML_NAMESPACE_END()

#endif
