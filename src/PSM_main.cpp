/**
Parallel Scissor Manipulator Robot (PSMR) Triple Scissor Extender (TSE) Main control code
Daniel J. Gonzalez - dgonz@mit.edu
9/14/2015
*/


#include "PSM_main.h"

// Comment this out to use EtherCAT
#define USE_CAN

/*
#include <cstdio>
#include <cstdlib>
#include <armadillo>
#include <iostream>
#include <fstream>  //THIS IS TO WRITE FILE
#include <cmath>

#include "CML.h"
*/

//Ball joint locations relative to BeT
mat eTp1(1,3);
mat eTp2(1,3);
mat eTp3(1,3);
//Rotation Matrices for coordinate shift
mat RzA = rotz(-pi/2);
mat RzB = rotz(pi/6);
mat RzC = rotz(5*pi/6);
mat topPtDist(1,3);
mat bA = RzA*topPtDist.t();
mat bB = RzB*topPtDist.t();
mat bC = RzC*topPtDist.t();
float pose[6]= {0, 0, 13, 0, 0, 0};

mat dAct(6,1);
mat JT(6,3);
mat dT(3,1);

/**************************************************
* Just home the motor and do a bunch of random
* moves.
**************************************************/
int main( void )
{
   cout << "Using Armadillo version: " << arma_version::as_string() << endl;

   //////////////////////////Precalculate important matrices//
   //Ball joint locations relative to BeT
   eTp1 << rT << 0 << -hT << endr;
   eTp1 = eTp1.t();    
   eTp2 << rT*cos(2*pi/3) << rT*sin(2*pi/3) << -hT << endr;
   eTp2 = eTp2.t(); 
   eTp3 << rT*cos(4*pi/3) << rT*sin(4*pi/3) << -hT << endr;
   eTp3 = eTp3.t();
   // cout << "eTp1: \n" << eTp1; //Print out eTp1 for testing

   //get center points of A, B, C in O frame: 
   
   topPtDist <<0 << tDIST << 0 << endr;

   /** 
   //TEST JACOBIAN
   Point<AMPCT> act;
   act[0] = 230;
   act[1] = 230;
   act[2] = 230;
   act[3] = 230;
   act[4] = 230;
   act[5] = 230;
   getTransJacobian(act, pose);
   */

   ofstream myfile;
   myfile.open ("results.txt");
   myfile << "-PSMR/TSE Control Code Output\n"<<
             "-Daniel J. Gonzalez - dgonz@mit.edu\n\n"<<
             "act0, act1, act2, act3, act4, act5, "<<
             "x, y, z, psi, theta, phi, "<<
             "dact0, dact1, dact2, dact3, dact4, dact5, "<<
             "dx, dy, dz, dpsi, dtheta, dphi\n\n";

   //cout << "ayy lmao\n"; //For debugging

   ////////////////////Set up Copley Libraries///////////
   // The libraries define one global object of type
   // CopleyMotionLibraries named cml.
   //
   // This object has a couple handy member functions
   // including this one which enables the generation of
   // a log file for debugging
   cml.SetDebugLevel( LOG_EVERYTHING );

   // Create an object used to access the low level CAN network.
   // This examples assumes that we're using the Copley PCI CAN card.
   #if defined( USE_CAN )
      KvaserCAN hw( "CAN0" );
      hw.SetBaud( canBPS );
   #elif defined( WIN32 )
      WinUdpEcatHardware hw( "eth0" );
   #else
      LinuxEcatHardware hw( "eth0" );
   #endif

      // Open the network object
   #if defined( USE_CAN )
      CanOpen net;
   #else
      EtherCAT net;
   #endif
   const Error *err = net.Open( hw );
   showerr( err, "Opening network" );

   // Initialize the amplifiers using default settings
   Amp amp[AMPCT];
   AmpSettings set;
   set.guardTime = 0;
   printf( "Doing init\n" );
   int i;
   for( i=0; i<AMPCT; i++ )
   {
      printf( "Initiating Amplifier %d\n", canNodeID+i );
      err = amp[i].Init( net, canNodeID+i, set );
      showerr( err, "Initting amp" );

      MtrInfo mtrInfo;
      err = amp[i].GetMtrInfo( mtrInfo );
      showerr( err, "Getting motor info\n" );

      // err = amp[i].SetCountsPerUnit( mtrInfo.ctsPerRev );
      // printf( "CountsPerRev %d\n", mtrInfo.ctsPerRev );

      // printf( "CountsPerRev %d\n", mtrInfo.ctsPerRev );

      err = amp[i].SetCountsPerUnit( 8000.0/5.0);     // User Units are now in mm

      // printf( "Doing init\n" );
      showerr( err, "Setting cpr\n" );
   }

   // Create a linkage object holding these amps
   Linkage link;
   err = link.Init( AMPCT, amp );
   showerr( err, "Linkage init" );

   // Home the amps
   HomeConfig hcfg;
   err = link[0].GetHomeConfig(hcfg);
   showerr(err, "get Home Config");
   hcfg.velFast = 5;
   hcfg.velSlow = 1;
   hcfg.accel = 10;
   hcfg.method  = CHM_NHOME_INDX;   // CHM_NONE;
   hcfg.offset  = 0;
   printf( "Home Velocity %f \n", hcfg.velFast );
   printf( "Home Velocity Slow %f \n", hcfg.velSlow );

   
   // Setup the velocity, acceleration, deceleration & jerk limits
   // for multi-axis moves using the linkage object
   err = link.SetMoveLimits( 75, 75, 75, 100 );
   showerr( err, "setting move limits" );

   // Create an N dimensional position to move to
   Point<AMPCT> act;

   for( i=0; i<AMPCT; i++ )
   {
      // Assuming we are low to the ground and centered, first move forward 30mm. 
      //Notice that the linkage object may be used like an array to reference the amplifiers.
      double currentPosition;
      err = link[i].GetPositionActual(currentPosition);
      printf( "Current Position %f \n", currentPosition );
      showerr( err, "ActualPosition" );
      
      act[i]= currentPosition+50; //in mm

      // if(currentPosition<0){
      //    act[i]= currentPosition+(0-currentPosition)+2.5; //in mm
      // }else{
      //    act[i]= 2.5;
      // }
      printf( "Goal Position %f mm \n", act[i]);
   }

   err = link.MoveTo( act );
   showerr( err, "Moving linkage" );

   // Wait for all amplifiers to finish the initial move by waiting on the
   // linkage object itself.
   printf( "Waiting for move up to finish...\n" );
   err = link.WaitMoveDone( 20000 ); 
   showerr( err, "waiting on initial move" );
   

   for( i=0; i<AMPCT; i++ )
   {
      // Home the amp.  Notice that the linkage object may be used 
      // like an array to reference the amplifiers.
      if(i==0){
         hcfg.offset  = 2;
      }else if(i==2 || i==3){
         hcfg.offset  = -0.75;
      }else if(i==4){
         hcfg.offset  = -1;
      }else{
         hcfg.offset  = 0;
      }

      err = link[i].GoHome(hcfg); // hcfg 
      showerr( err, "Going home" );
   }

   // Wait for all amplifiers to finish the home by waiting on the
   // linkage object itself.
   printf( "Waiting for home to finish...\n" );
   err = link.WaitMoveDone( 20000 ); 
   showerr( err, "waiting on home" );

   /*******************************************************************/

   act[0] = 230;
   act[1] = 230;
   act[2] = 230;
   act[3] = 230;
   act[4] = 230;
   act[5] = 230;

   err = link.MoveTo( act );
   showerr( err, "Moving linkage" );

   // Wait for all amplifiers to finish the initial move by waiting on the
   // linkage object itself.
   printf( "Waiting for move up to finish...\n" );
   err = link.WaitMoveDone( 20000 ); 
   showerr( err, "waiting on initial move" );

   act[0] = 100;
   act[1] = 100;
   act[2] = 100;
   act[3] = 100;
   act[4] = 100;
   act[5] = 100;

   err = link.MoveTo( act );
   showerr( err, "Moving linkage" );

   // Wait for all amplifiers to finish the initial move by waiting on the
   // linkage object itself.
   printf( "Waiting for move up to finish...\n" );
   err = link.WaitMoveDone( 20000 ); 
   showerr( err, "waiting on initial move" );

   /********************************************************************/
   // Move using the Jacobian

   pose[2] = 24; //Inches

   //test vertical motion by moving up a quarter inch
   JT = getTransJacobian(act, pose);
   dT << 0 << endr << 0 << endr << .25 << endr;
   dAct = JT*dT;


   for(int count = 0; count < 6; count ++){
            myfile << act[count] << ", " ;
   }
   for(int count = 0; count < 6; count ++){
            myfile << pose[count] << ", " ;
   }
   myfile << dAct << ", "<< dT << "\n";

   act[0] = act[0] - dAct(0)*in2mm;
   act[1] = act[1] - dAct(1)*in2mm;
   act[2] = act[2] - dAct(2)*in2mm;
   act[3] = act[3] - dAct(3)*in2mm;
   act[4] = act[4] - dAct(4)*in2mm;
   act[5] = act[5] - dAct(5)*in2mm;
   pose[2] = pose[2]+.25;


   err = link.MoveTo( act );
   showerr( err, "Moving linkage" );

   // Wait for all amplifiers to finish the initial move by waiting on the
   // linkage object itself.
   printf( "Waiting for move up to finish...\n" );
   err = link.WaitMoveDone( 20000 ); 
   showerr( err, "waiting on initial move" );
   cout << "Press ENTER to coninue...";
   std::cin.ignore();

   //Return
   JT = getTransJacobian(act, pose);
   dT << 0 << endr << 0 << endr << -.25 << endr;
   dAct = JT*dT;

   ////////////////WRITE TO FILE///////////
   for(int count = 0; count < 6; count ++){
            myfile << act[count] << ", " ;
   }
   for(int count = 0; count < 6; count ++){
            myfile << pose[count] << ", " ;
   }
   myfile << dAct << ", "<< dT << "\n";
   ////////////////////////////////////////

   act[0] = act[0] - dAct(0)*in2mm;
   act[1] = act[1] - dAct(1)*in2mm;
   act[2] = act[2] - dAct(2)*in2mm;
   act[3] = act[3] - dAct(3)*in2mm;
   act[4] = act[4] - dAct(4)*in2mm;
   act[5] = act[5] - dAct(5)*in2mm;
   pose[2] = pose[2]-.25;

   err = link.MoveTo( act );
   showerr( err, "Moving linkage" );

   // Wait for all amplifiers to finish the initial move by waiting on the
   // linkage object itself.
   printf( "Waiting for move up to finish...\n" );
   err = link.WaitMoveDone( 20000 ); 
   showerr( err, "waiting on initial move" );
   cout << "Press ENTER to coninue...";
   std::cin.ignore();

   /*
   for( int j=0; j<50; j++ )
   {
      // Create an N dimensional position to move to
      Point<AMPCT> act;

      printf( "%d: moving to ", j );
      for( i=0; i<AMPCT; i++ ){
         double currentPosition;
         err = link[i].GetPositionActual(currentPosition);
         showerr( err, "ActualPosition" );
         act[i] = 150+90*(rand() % 100000) / 100000.0;
         //if( i ) act[i] = act[0];
         printf( "%.3lf ", act[i] );
      }
      printf( "\n" );

      // This function will cause the linkage object to create a 
      // multi-axis S-curve move to the new position.  This 
      // trajectory will be passed down to the amplifiers using
      // the PVT trajectory mode
      err = link.MoveTo( act );
      showerr( err, "Moving linkage" );

      // Wait for the move to finish
      err = link.WaitMoveDone( 1000 * 30 );
      showerr( err, "waiting on linkage done" );
   }
   */

   /*****************************************************************/

   act[0] = 0;
   act[1] = 0;
   act[2] = 0;
   act[3] = 0;
   act[4] = 0;
   act[5] = 0;

   err = link.MoveTo( act );
   showerr( err, "Moving linkage" );

   // Wait for all amplifiers to finish the initial move by waiting on the
   // linkage object itself.
   printf( "Waiting for move up to finish...\n" );
   err = link.WaitMoveDone( 20000 ); 
   showerr( err, "waiting on initial move" );
   
   myfile.close();

   return 0;
}

/**************************************************/

static void showerr( const Error *err, const char *str )
{
   if( err )
   {
      printf( "Error %s: %s\n", str, err->toString() );
      exit(1);
   }
}

mat rotx(float myAng){
   mat output(3,3);
   output << 1 << 0          << 0           << endr
          << 0 << cos(myAng) << -sin(myAng) << endr
          << 0 << sin(myAng) << cos(myAng)  << endr;
   return output;
}

mat roty(float myAng){
   mat output(3,3);
   output << cos(myAng)  << 0 << sin(myAng) << endr
          << 0           << 1 << 0          << endr
          << -sin(myAng) << 0 << cos(myAng) << endr;
   return output;
}

mat rotz(float myAng){
   mat output(3,3);
   output << cos(myAng) << -sin(myAng) << 0 << endr
          << sin(myAng) << cos(myAng)  << 0 << endr
          << 0          << 0           << 1 << endr;
   return output;
}

mat getTransJacobian(Point<6> act, float pose[6]){

   //act[6] are the 6 actuator positions in inertial frame
   //BeT is the 6DOF pose 
   //[x (inches), y, z, psi (degrees), theta, phi]'
   //Yaw about Z, Pitch about Y', Roll about X''

   mat BeT(1,6);
   //Convert act to sigmas
   BeT << pose[0] << 
          pose[1] <<
          pose[2] << 
          pose[3] <<
          pose[4] <<
          pose[5] << endr; 

   //Define main state equations
   mat sigma(1, 6);

   //Convert act to sigmas
   sigma << DIST2+12-act[5]*mm2in << 
            DIST2+12-act[0]*mm2in << 
            DIST2+12-act[1]*mm2in << 
            DIST2+12-act[2]*mm2in << 
            DIST2+12-act[3]*mm2in << 
            DIST2+12-act[4]*mm2in <<endr; 

   mat Rz = rotz(BeT[3]);
   mat Ry = roty(BeT[4]);
   mat Rx = rotx(BeT[5]);

   mat BTp1 = BeT.submat(0,0,0,2).t()+ Rx*Ry*Rz*eTp1; 
   mat BTp2 = BeT.submat(0,0,0,2).t()+ Rx*Ry*Rz*eTp2; 
   mat BTp3 = BeT.submat(0,0,0,2).t()+ Rx*Ry*Rz*eTp3;


   //Transform points from Origin to A, B, C
   //to convert a point in O to A: 
   //Subtract by bA, multiply by inverse rotation matrix. 
   mat tA = solve(RzA,(BTp1 - bA));
   mat tB = solve(RzB,(BTp2 - bB));
   mat tC = solve(RzC,(BTp3 - bC));

   /**  Jacobian for A, B, C: **/
   mat JSA = getSubJacobian(tA, sigma.submat(0,0,0,1));
   mat JSB = getSubJacobian(tB, sigma.submat(0,2,0,3));
   mat JSC = getSubJacobian(tC, sigma.submat(0,4,0,5));
   
   //  Translational Jacobian
   mat JeT(9,6);
   JeT = join_vert(join_vert(
          join_horiz(eye<mat>(3,3), zeros<mat>(3,3)),
          join_horiz(eye<mat>(3,3), zeros<mat>(3,3))),
          join_horiz(eye<mat>(3,3), zeros<mat>(3,3)));

   mat JIT(6,9);

   /*
   cout << JSA*RzA << "\n";
   cout << zeros<mat>(2,6) << "\n";
   cout << join_horiz(JSA*RzA, zeros<mat>(2,6)) << "\n";
   */


   JIT = join_vert(
            join_vert(
               join_horiz(
                  JSA*RzA, zeros<mat>(2,6)
                  ),
               join_horiz(
                  join_horiz(
                     zeros<mat>(2,3),
                     JSB*RzB
                     ),
                  zeros<mat>(2,3)
                  )
               ),
            join_horiz(
               zeros<mat>(2,6), 
               JSC*RzC
               )
            );
   
   JIT = JIT*JeT;
   JIT = JIT.submat(0,0,5,2); //(:,1:3);

   //cout << JIT << "\n";
   return JIT; 
}

mat getSubJacobian(mat t, mat sigma){
   //Given: 
   float xt = t(0);
   float yt = t(1);
   float zt = t(2);
   float sigma1 = sigma(0);
   float sigma2 = sigma(1);


   float genA = 2*sigma1 - yt - sqrt(3)*xt;
   float genB = sigma2-sigma1;
   float genC = yt-2*sigma2-sqrt(3)*xt;
   float genD = sqrt(3)*(sigma1+sigma2);
   float genE = 2*zt; 
   float genF = -(2*sigma1-sqrt(3)*xt-yt)+(1-pow((L/l_0),2))*(.5*sigma1+0.25*sigma2);
   float genG = sigma1*sqrt(3)+2*xt;
   float genH = sigma1+2*yt;
   float genK = (1-pow((L/l_0),2))*(.5*sigma2+0.25*sigma1);

   float dSigma1dXt = (genC*genG+genK*genD)/(genK*genA-genC*genF);
   float dSigma1dYt = (genC*genH+genK*genB)/(genK*genA-genC*genF);
   float dSigma1dZt = (genC*genE)/(genC*genF-genK*genA);
   float dSigma2dXt = (genF*genD+genG*genA)/(genF*genC-genK*genA);
   float dSigma2dYt = (genH*genA+genF*genB)/(genF*genC-genK*genA);
   float dSigma2dZt = (genA*genE)/(genK*genA-genF*genC);
   
   mat JS(2,3);
   JS << dSigma1dXt << dSigma1dYt << dSigma1dZt << endr
       << dSigma2dXt << dSigma2dYt << dSigma2dZt << endr;
   return JS; 
}