/**
Parallel Scissor Manipulator Robot (PSMR) Triple Scissor Extender (TSE) Main control code
Daniel J. Gonzalez - dgonz@mit.edu
9/14/2015
*/


#include "PSM_main.h"

// Comment this out to use EtherCAT
#define USE_CAN

int main( void )
{

   ////////////////////////////////////////////////////////////////////////////////////////// VVV Initialization and Homing VVV
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

   const Error *err;
   int i;
   Amp amp[AMPCT];
   if(robotPlugged){
      err = net.Open( hw );
      showerr( err, "Opening network" );

      // Initialize the amplifiers using default settings
      AmpSettings set;
      set.guardTime = 0;

      cout << "Doing initialization"<<endl;
      for( i=0; i<AMPCT; i++ )
      {
         //printf( "Initiating Amplifier %d\n", canNodeID+i );
         cout << "Initializing Amplifier " << (canNodeID+i) << endl;

         err = amp[i].Init( net, canNodeID+i, set );
         showerr( err, "Initting amp" );

         MtrInfo mtrInfo;
         err = amp[i].GetMtrInfo( mtrInfo );
         showerr( err, "Getting motor info\n" );

         // err = amp[i].SetCountsPerUnit( mtrInfo.ctsPerRev );
         // printf( "CountsPerRev %d\n", mtrInfo.ctsPerRev );

         err = amp[i].SetCountsPerUnit( 8000.0/5.0);     // User Units are now in mm
         showerr( err, "Setting cpr\n" );
      }
   }
      // Create a linkage object holding these amps
      Linkage link;

   if(robotPlugged){
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
   }
   
   ////////////////////////////////////////////////////////////////////////////////////////// ^^^ Initialization and Homing Complete. ^^^

   // Create an N dimensional position to move to
   Point<AMPCT> act;

   act[0] = 99-1;
   act[1] = 99-2;
   act[2] = 99-2.5;
   act[3] = 99-1;
   act[4] = 99-2;
   act[5] = 99-.5;
   // Z is 45.75 inches


   if(robotPlugged){
      err = link.MoveTo( act );
      showerr( err, "Moving linkage" );
      // Wait for all amplifiers to finish the initial move by waiting on the
      // linkage object itself.
      printf( "Waiting for move up to finish...\n" );
      err = link.WaitMoveDone( 20000 ); 
      showerr( err, "waiting on initial move" );
   }

   // Create an N dimensional slide vector
   // Point<AMPCT> q;
   double q[AMPCT];
   char msgBack;
   char qMsg[sizeof(double)];
   double qTemp;

   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////Main Function
   while(isRunning){
      std::cout<< "RUN CHECK\n";

      msgBack = std::cin.get();
      std::cout<< "CPP Receieves Message!\n";
      isRunning = msgBack-48;

      if(isRunning){
         std::cout<< "RUN OK\n";

         //Check if valid q. If not, then isRunning = false, break. Or, try again. 
         int checker = 0;
         for (int i = 0; i<AMPCT; i++){
            std::cout<< "GIMME\n";

            for (int j = 0; j<sizeof(double); j++){
                  qMsg[j] = std::cin.get();
                  std::cout<< qMsg[j] <<endl;
            }

            //printf("Received:%s\n", qMsg );

            memcpy(&qTemp,&qMsg,sizeof(double));
            printf( "Converted %f \n", qTemp);

            q[i] = qTemp;
            
            if(SIGMA2ACTUATOR - q[i]<=250 && SIGMA2ACTUATOR - q[i] >= 0){
               checker++;
            }
         }
         for (int i = 0; i<AMPCT; i+=2){
            if(sqrt(q[i]*q[i]+q[i]*q[i+1]+q[i+1]*q[i+1])<=MAXWIDTH){
               checker+=2;
            }
         }

         if(checker == 2*AMPCT){
            checker = 0;
            std::cout<< "GOOD Q\n";
            std::cout<< "Moving to point...\n";
            //If all safe, Assign vector act[] with the new coords. If not, stay. 
            //Convert from q (sigma coords) to actuator coords (0 to 300mm)
            act[0] = SIGMA2ACTUATOR - q[0];
            act[1] = SIGMA2ACTUATOR - q[1];
            act[2] = SIGMA2ACTUATOR - q[2];
            act[3] = SIGMA2ACTUATOR - q[3];
            act[4] = SIGMA2ACTUATOR - q[4];
            act[5] = SIGMA2ACTUATOR - q[5];
            if(robotPlugged){
               err = link.MoveTo( act );
               showerr( err, "Moving linkage" );

               // Wait for all amplifiers to finish the initial move by waiting on the
               // linkage object itself.
               printf( "Waiting for move up to finish...\n" );
               err = link.WaitMoveDone( 20000 ); 
               showerr( err, "waiting on initial move" );
            }
         }else{
            checker = 0;
            std::cout<< "BAD Q\n";
            std::cout<< "Please try again...\n";
         }      
         
         // tell Python we're done with this move. 
         std::cout<<"Move Done.\n";
      }else{
         std::cout<<"Ending Program...\n";
      }

   }

   //////////////////////////////////////////////////////////////////////////////////////Go to home position before ending

   act[0] = 0;
   act[1] = 0;
   act[2] = 0;
   act[3] = 0;
   act[4] = 0;
   act[5] = 0;

   //cout << "Press ENTER to coninue...";
   //std::cin.ignore();
   if(robotPlugged){
      err = link.MoveTo( act );
      showerr( err, "Moving linkage" );

      // Wait for all amplifiers to finish the initial move by waiting on the
      // linkage object itself.
      printf( "Waiting for move up to finish...\n" );
      err = link.WaitMoveDone( 20000 ); 
      showerr( err, "waiting on initial move" );
   }
   
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