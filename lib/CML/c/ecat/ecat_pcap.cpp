#include <stdio.h>
#include <string.h>

#define HAVE_REMOTE
#include <pcap.h>

#include "CML.h"
#include "ecat_pcap.h"

CML_NAMESPACE_USE();

#define PCAP_TIMEOUT         50

/**
   Create an EtherCAT hardware interface which uses the pcap library to
   capture packets.  This module also supports the winpcap library on
   Windows.  Please see http://www.winpcap.org for more information.

   @param name The parameter identifies the Ethernet adapter to open.
               This parameter can either be the name of an EtherNet port
               as returned by the pcap call pcap_findalldevs, or it 
               can take the form ethN for some integer N (i.e. eth0, eth1, etc).
               In this case the Nth Ethernet port identified by the pcap 
               library will be used.

               If NULL Is passed (default), then the first Ethernet adapter 
               will be used.
*/
PcapEcatHardware::PcapEcatHardware( const char *name )
{
   if( !name ) name = "eth0";
   devList = 0;
   hndl = 0;
   ifname = name;
   SetRefName( "PcapEcatHw" );
}

PcapEcatHardware::~PcapEcatHardware( void )
{
   KillRef();

   if( devList )
   {
      pcap_freealldevs( (pcap_if_t *)devList );
      devList=0;
   }
   Close();
}

const Error *PcapEcatHardware::FindDevNames( void )
{
   pcap_if_t *alldevs;
   char errbuf[PCAP_ERRBUF_SIZE];

   // Just quit if this has already been done
   if( devList ) return 0;

   // Retrieve the device list on the local machine
   if( pcap_findalldevs( &alldevs, errbuf ) )
   {
      cml.Error( "Error calling pcap_findalldevs: %s\n", errbuf );
      return &EtherCatError::OpenHardware;
   }

   devList = alldevs;
   return 0;
}

const Error *PcapEcatHardware::Open( void )
{
   pcap_if_t *d, *alldevs;
   char errbuf[PCAP_ERRBUF_SIZE];

   // Retrieve the device list on the local machine
   const Error *err = FindDevNames();
   if( err ) return err;

   // First, see if the input string is an exact match for a device name
   alldevs = (pcap_if_t*)devList;
   for( d=alldevs; d; d=d->next )
   {
      if( !strcmp( d->name, ifname ) )
         break;
   }

   if( !d )
   {
      // See if the input name was of the form 'ethN'.  If so, match the Nth 
      // device on this list.
      int num;
      if( sscanf( ifname, "eth%d", &num ) == 1 )
      {
         for( d=alldevs; d; d=d->next, num-- )
         {
            if( !num )
               break;
         }
      }
   }

   // Fail if we couldn't locate the requested device
   if( !d )
   {
      cml.Error( "Unable to locate Pcap device: %s\n", ifname );
      return &EtherCatError::OpenHardware;
   }

   cml.Debug( "Opening Ethernet adapter: %s\n", d->description );

   // Open the device in promiscuous mode
   hndl = pcap_open_live( d->name, 65536, 1, PCAP_TIMEOUT, errbuf );

   if( !hndl )
   {
      cml.Error( "Unable to open Ethernet port, pcap_open failed with error %s\n", errbuf );
      return &EtherCatError::OpenHardware;
   }

   pcap_setmintocopy( (pcap_t *)hndl, 60 );
   pcap_setbuff( (pcap_t *)hndl, 10000000 );

   return 0;
}

const Error *PcapEcatHardware::Close( void )
{
   if( hndl )
   {
      pcap_close( (pcap_t*)hndl );
      hndl = 0;
   }
   return 0;
}

/**
 * Return the name of the Nth EtherCAT adapter available in the system.
 * @param index Identifies which adapter name to return.
 * @return A locally allocated buffer holding the name, or NULL if there
 *         are no remaining adapters available.  The returned buffer is
 *         owned by the class and will be updated on subsequent calls to 
 *         this function.
 */
const char *PcapEcatHardware::GetAdapterName( int index )
{
   const Error *err = FindDevNames();
   if( err ) return 0;

   pcap_if_t *d, *alldevs = (pcap_if_t*)devList;
   for( d=alldevs; index && d; d=d->next, index-- );

   if( d ) return d->name;
   return 0;
}

/**
 * Return a description of the Nth EtherCAT adapter available in the system.
 * @param index Identifies which adapter name to return.
 * @return A locally allocated buffer holding the description, or NULL if there
 *         is no description available.  It's possible that a valid device number
 *         may not have a description available, so a NULL returned here does not
 *         necessarily indicate the lack of a device.
 */
const char *PcapEcatHardware::GetAdapterDesc( int index )
{
   const Error *err = FindDevNames();
   if( err ) return 0;

   pcap_if_t *d, *alldevs = (pcap_if_t*)devList;
   for( d=alldevs; index && d; d=d->next, index-- );

   if( d ) return d->description;
   return 0;
}

const Error *PcapEcatHardware::SendPacket( uchar *msg, uint16 len )
{
   CML_ASSERT( msg != 0 );
   CML_ASSERT( len > 14 );

   if( !hndl ) return &EtherCatError::EcatNotInit;

   if( pcap_sendpacket( (pcap_t*)hndl, msg, len ) )
   {
      cml.Error( "Error writing to ethernet socket: %s\n", 
                 pcap_geterr( (pcap_t*)hndl ) );
      return &EtherCatError::WriteHardware;
   }
   return 0;
}

const Error *PcapEcatHardware::RecvPacket( uchar *msg, uint16 *len, Timeout to )
{
   int32 timeout = (int32)to;

   char errbuf[PCAP_ERRBUF_SIZE];
   if( !hndl ) return &EtherCatError::EcatNotInit;

   // Set the interface to non-blocking if the timeout was zero.
   pcap_setnonblock( (pcap_t *)hndl, (timeout==0), errbuf );

   // Unfortunately, pcap doesn't support changing the timeout from the value I set
   // when I opened the interface.  I'll have to poll to emulate a timeout
   while( 1 )
   {
      struct pcap_pkthdr *pkt_header;
      const u_char *pkt_data;

      int ret = pcap_next_ex( (pcap_t *)hndl, &pkt_header, &pkt_data );

      switch( ret )
      {
         // Timeout
         case 0:
         {
            if( timeout < 0 )
               continue;

            if( timeout <= PCAP_TIMEOUT )
               return &ThreadError::Timeout;

            timeout -= PCAP_TIMEOUT;
            continue;
         }

            // Success
         case 1:
         {
            uint32 ct;

            if( pkt_header->caplen > *len )
               ct = *len;
            else
               ct = pkt_header->caplen;

            memcpy( msg, pkt_data, ct );
            *len = ct;
            return 0;
         }

            // Error
         default:
            cml.Error( "Error reading from ethernet socket: %s\n", 
                       pcap_geterr( (pcap_t*)hndl ) );
            return &EtherCatError::ReadHardware;
      }
   }

   return 0;
}
