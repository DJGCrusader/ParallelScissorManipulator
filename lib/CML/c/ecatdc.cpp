/*******************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2011 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/** \file

This file holds some utility code used by the EtherCAT network
when initializing it's distributed clock.
*/

#include <string.h>
#include <time.h>
#include "CML.h"

CML_NAMESPACE_USE();

struct DcNodeInfo
{
   int branches;
   int64 ecTime;
   int32 delay[3];
   int32 propDelay;
   int32 msgDelay;

   DcNodeInfo( void )
   {
      branches = 0;
      ecTime = 0;
      propDelay = 0;
      msgDelay = 0;
      for( int i=0; i<3; i++ )
         delay[i] = 0;
   }

   const Error *Init( int id, uint16 dlStat, uint32 t[4], int64 ec );
   DcNodeInfo *FindDelay( int32 inDelay, bool inProc, DcNodeInfo *next, int &remain );
   DcNodeInfo *SumDelay( int32 &inDelay, DcNodeInfo *next, int &remain );
};

/// Initialize the distributed clock system on this network.
/// This is called at startup by EtherCAT::Open();
/// @return An error object, or NULL on success
const Error *EtherCAT::InitDistClk( void )
{
   // Find the current time as a number of ns since 1/1/2000.
   struct tm epoch;
   memset( &epoch, 0, sizeof(epoch) );
   epoch.tm_mday = 1;
   epoch.tm_year = 100;
   epoch.tm_isdst = -1;
   int64 now = (int64)(difftime( time(0), mktime(&epoch) ) * 1000000000.0);

   // Do a broadcast write to all devices on the network which causes them
   // to latch a timestamp on each Ethernet port
   EcatFrame frame;
   BWR bwr( 0x900, 1, 0 );

   frame.Reset();
   frame.Add( &bwr );
   const Error *err = SendFrame( &frame, 500 );
   if( err ) return err;

   refClkNode = -1;

   bool netOK = true;
   DcNodeInfo *ndInfo = new DcNodeInfo[nodeCt];

   // Now, read the latched timestamps
   // For the moment I only support simple networks with no branching.
   cml.Debug( "Finding DC delay times for all nodes\n" );
   int i;
   for( i=0; i<nodeCt; i++ )
   {
      frame.Reset();
      APRD dl( i, 0x110, 2 );
      APRD p0( i, 0x900, 4 );
      APRD p1( i, 0x904, 4 );
      APRD p2( i, 0x908, 4 );
      APRD p3( i, 0x90C, 4 );
      APRD ec( i, 0x918, 8 );
      APWR rst( i, 0x928, 4, 0 );
      frame.Add( &dl );
      frame.Add( &p0 );
      frame.Add( &p1 );
      frame.Add( &p2 );
      frame.Add( &p3 );
      frame.Add( &ec );
      frame.Add( &rst );
      err = SendFrame( &frame, 500 );
      if( err )
      {
         delete[] ndInfo;
         return err;
      }

      if( (refClkNode < 0) && p0.getWKT() ) refClkNode = i;

      uint16 dlStat = dl.getData16u();

      uint32 t[4];
      t[0] = p0.getData32u();
      t[1] = p1.getData32u();
      t[2] = p2.getData32u();
      t[3] = p3.getData32u();

      int64 ecTime;
      ec.getData( &ecTime, 8 );

      const Error *err = ndInfo[i].Init( i, dlStat, t, ecTime );
      if( err )
      {
         cml.Error( "Error: network wiring error at node %d while calculating distributed clock\n", i );
         netOK = false;
      }

      // Find an offset between the current time and the node's system time
      int64 diff = now - ndInfo[i].ecTime;
      if( diff < 0 ) diff = 0;

      frame.Reset();
      APWR off( i, 0x920, 8, &diff );
      frame.Add( &off );
      err = SendFrame( &frame, 500 );
      if( err )
      {
         delete[] ndInfo;
         return err;
      }
   }

   // Just quit on a wiring error.  I keep going with zero propagation delay settings.
   // This still works well enough for most systems.
   if( !netOK )
   {
      delete[] ndInfo;
      return 0;
   }

   cml.Debug( "Calculated node branch delays:\n" );
   for( i=0; i<nodeCt; i++ )
   {
      cml.Debug( "Node %d: ", i );
      for( int j=0; j<ndInfo[i].branches; j++ )
         cml.Debug( "%8d ", ndInfo[i].delay[j] );
      cml.Debug( "\n" );
   }

   // Find the propagation delay on each node
   int remain = nodeCt-1;
   ndInfo[0].FindDelay( 0, false, &ndInfo[1], remain );

   remain = nodeCt-1;
   int d = 0;
   ndInfo[0].SumDelay( d, &ndInfo[1], remain );

   for( i=0; i<nodeCt; i++ )
   {
      cml.Debug( "Node %d propagation delay calculated as %dns\n", i, ndInfo[i].propDelay );
      frame.Reset();
      APWR delay( i, 0x928, 4, ndInfo[i].msgDelay );
      frame.Add( &delay );
      err = SendFrame( &frame, 500 );
      if( err ) break;
   }

   delete[] ndInfo;
   return err;
}

/**
Init the node info object.
@param dlStat  The data link status for this node
@param t       Array of port receive times.
@return An error object, or NULL on success
*/
const Error *DcNodeInfo::Init( int id, uint16 dlStat, uint32 t[4], int64 ec )
{
   ecTime = ec;

   int i, mask = 0x100;
   for( i=0; i<4; i++, mask<<=2 )
      if( mask & dlStat ) t[i] = 0;

   cml.Debug( "Node %d, DL stat: 0x%04x Receive times: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", id, dlStat, t[0], t[1], t[2], t[3] );

   switch( (~dlStat) & 0x5500 )
   {
      // Port 0 is the only port open.  This is the end of a branch
      case 0x0100: branches = 0; break;

      // Ports 1, 2, or 3 are the only ones open.  Technically a wiring error, but I'll accept it.
      case 0x0400: branches = 0; break;
      case 0x1000: branches = 0; break;
      case 0x4000: branches = 0; break;

      // Port 0 and one other port open.
      case 0x0500: branches = 1; delay[0]=t[1]-t[0]; break; // 0 & 1
      case 0x1100: branches = 1; delay[0]=t[2]-t[0]; break; // 0 & 2
      case 0x4100: branches = 1; delay[0]=t[3]-t[0]; break; // 0 & 3

      // Port 0 and two other ports open.
      case 0x1500: branches = 2; delay[0]=t[1]-t[0]; delay[1]=t[2]-t[1]; break; // 0 & 1 & 2
      case 0x4500: branches = 2; delay[0]=t[3]-t[0]; delay[1]=t[1]-t[3]; break; // 0 & 1 & 3
      case 0x5100: branches = 2; delay[0]=t[3]-t[0]; delay[1]=t[2]-t[3]; break; // 0 & 2 & 3

      // All ports open
      case 0x5500: branches = 3; delay[0]=t[3]-t[0]; delay[1]=t[1]-t[3]; delay[1]=t[2]-t[1]; break;

      // Port 0 is closed, but other ports are open.  This is a wiring error that I don't currently handle
      default:
         cml.Warn( "Wiring error on node %d, port 0 is closed, but other ports are open.  Aborting prop delay calculations.\n", id );
         return &EtherCatError::NetworkWiringError;
   }

   // Look through the delays I've calculated and make sure that none are negative.
   // A negative delay means that some port other then 0 is pointing at the master.
   for( i=0; i<branches; i++ )
   {
      if( delay[i] < 0 )
      {
         cml.Warn( "Negative branch delay on node %d indicates wiring error.  Aborting prop delay calculations.\n", id );
         return &EtherCatError::NetworkWiringError;
      }
   }

   return 0;
}

/* Find the propagation delay from the previous node to this one.
   @param inDelay The total delay measured by the previous node on this node's branch.
                  This delay includes the processing (or forwarding) delay of the previous node, any 
                  propagation delay to & from this node, plus any delays in this node and those beyond.
   @param inProc  If true, the input delay includes processing.  If false, it only includes forwarding.
   @param next    Pointer to the next node after this one in the network.  This is an array of nodes.
   @param remain  The total number of nodes in the array.  On return, the number of unprocessed nodes is stored here.
   @return A pointer to the next unprocessed node in the array.
*/
DcNodeInfo *DcNodeInfo::FindDelay( int32 inDelay, bool inProc, DcNodeInfo *next, int &remain )
{
   // Difference (in ns) between processing and forwarding delay.
   // This is from the ESC datasheet and should be constant for all ESC types.
   int Tdiff = 40;

   // If this node is the end of the branch, or there are no remaining 
   // nodes after it, the prop delay is simply half the input
   if( (branches < 1) || (remain<1) )
   {
      propDelay = inDelay/2;
      return next;
   }

   // For each branch, find the delay of the next node down that branch
   int Tn = 0;
   for( int i=0; i<branches; i++ )
   {
      Tn += delay[i];

      remain--;
      next = next->FindDelay( delay[i], (i==0), &next[1], remain );
   }

   // The input delay includes the following times:
   //   Tp or Tf - Processing or forwarding delay from the previous node
   //   Tw - Wire delay to this node
   //   Tw - Wire delay from this node
   //   Tf - Forwarding delay back through this node
   //   Tn - Measured delay of all branches beyond this node.  This value is known
   //
   // The propagation delay I'm trying to find is the delay between the previous node
   // receiving the message, and this node receiving the message.  That delay is 
   // Tw + Tp if the previous node processed the message, or Tw + Tf if the previous node
   // just forwarded the message.
   //
   // We don't know what Tp or Tf are, but we do know Tdiff=Tp-Tf from the ESC datasheet
   //
   // If the previous node processed the message, then the input delay is:
   //    inDelay = Tp + 2*Tw + Tn + Tf
   //            = Tp + 2*Tw + Tn + Tp - Tdiff
   //            = 2*Tp + 2*Tw + Tn - Tdiff
   //
   //    inDelay-Tn+Tdiff = 2*(Tp+Tw)
   //    propDelay = Tp+Tw = (inDelay-Tn+Tdiff)/2
   //
   //  If the previous node didn't process the message, the the input delay is:
   //    inDelay = 2*Tw + 2*Tf + Tn 
   //
   //    propDelay = Tw+Tf = (inDelay-Tn)/2
   propDelay = (inDelay-Tn)/2;
   if( inProc ) propDelay += Tdiff/2;

   // Quick sanity check.  This is useful for the first node since we pass in an inDelay
   // of zero which means we would end up with a negative propegation delay.
   if( propDelay < 0 ) propDelay = 0;

   return next;
}

/// Find the total delay of a message getting to this node
DcNodeInfo *DcNodeInfo::SumDelay( int32 &inDelay, DcNodeInfo *next, int &remain )
{
   inDelay += propDelay;
   msgDelay = inDelay;

   for( int i=0; i<branches; i++ )
   {
      remain--;
      next = next->SumDelay( inDelay, &next[1], remain );
      inDelay += propDelay;
   }

   return next;
}
