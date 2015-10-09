/**
Parallel Scisssor Manipulator Robot (PSMR) Header File
Daniel J. Gonzalez - dgonz@mit.edu
*/

// Comment this out to use EtherCAT
#define USE_CAN

#include <cstdio>
#include <cstdlib>
#include <armadillo>
#include <iostream>
#include <fstream>  //THIS IS TO WRITE FILE
#include <cmath>

#include "CML.h"

#if defined( USE_CAN )
#include "can/can_kvaser.h"   // formerly can_copley.h
#elif defined( WIN32 )
#include "ecat/ecat_winudp.h"
#else
#include "ecat/ecat_linux.h"
#endif

#define PI 3.14159265

float pi = PI;

//Dimensions
float in2mm = 25.4; 
float mm2in = 1.0/in2mm;
bool toPlot = true;

//Bottom Platform: 
float rA = 9.0683; //14.5; distance of linear guide from center
float rB = 28; 
float dA = 4.370; //distance between parallel linear guides

float hA = 40*mm2in; //height of the linear guide
float hJ = 1; //height to ball joint from top of linear guide
//float hB = hA+hJ; //height from base top surface to ball joint
float hB = 3.5231; 
float sA = 12; //stroke of the linear guide

float rDIST = 1.2615; //radial only distance of scissor point from center
float tDIST = 2.5230; //Distance of scissor coordinate system point t from center
float dist1 = 10.4240; //Distance of actuator from inner end of actuator
float DIST2 = 5.8172; //Distance of inner end of actuator to point t

float distH = (173.26+326.02+100-76)*mm2in;//22.5 inches; //sigma at 0mm

//Scissors:
float l_0 = 18;
float l_1 = 14;
float l_2 = 11;
float L = l_0 + l_1*2+ l_2*2;

//Top Platform
float rT = 8; //Radius from center of top to ball joint
float hT = 2; //2.125; //Distance from top to ball joint
//float pose[6];

// If a namespace has been defined in CML_Settings.h, this
// macros starts using it. 
CML_NAMESPACE_USE();
using namespace std;
using namespace arma;

/* local functions */
static int RunTest( void );
static void showerr( const Error *err, const char *str );

/* local defines */
#define AMPCT 6

/* local data */
int32 canBPS = 1000000;             // CAN network bit rate
const char *canDevice = "CAN0";           // Identifies the CAN device, if necessary
int16 canNodeID = 1;                // CANopen node ID of first amp.  Second will be ID+1, etc.

mat rotx(float myAngle);
mat roty(float myAngle);
mat rotz(float myAngle);

mat getTransJacobian(Point<6> act, float pose[6]);
mat getSubJacobian(mat tA, mat sigma);
int writeState(Point<6> act, float pose[6], mat dAct, mat dT, mat dR);