#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "CML.h"
#include "ecat_linux.h"

CML_NAMESPACE_USE();

#define ETHERCAT_PROTOCOL    0x88A4

LinuxEcatHardware::LinuxEcatHardware( const char *name )
{
   if( !name ) name = "eth0";

   fd = -1;
   ifname = CloneString( name );

   SetRefName( "LinuxEcatHw" );
}

LinuxEcatHardware::~LinuxEcatHardware( void )
{
   KillRef();
   Close();
   delete ifname;
   ifname = 0;
}

const Error *LinuxEcatHardware::Open( void )
{
   // If the port is already open, just return success
   if( fd >= 0 ) return 0;

   fd = socket( PF_PACKET, SOCK_RAW, htons(ETHERCAT_PROTOCOL) );

   if( fd < 0 )
   {
      cml.Error( "Error opening raw Ethernet socket: %s\n", strerror(errno) );
      return &EtherCatError::OpenHardware;
   }

   // Find the interface number associated with my interface
   struct ifreq r;
   strcpy( r.ifr_name, ifname );
   if( ioctl( fd, SIOCGIFINDEX, &r ) < 0 )
   {
      cml.Error( "Error looking up interface number for interface %s\n  %s\n", ifname, strerror(errno) );
      Close();
      return &EtherCatError::OpenHardware;
   }

   ifindex = r.ifr_ifindex;

   struct sockaddr_ll ll;
   ll.sll_family = AF_PACKET;
   ll.sll_protocol = htons(ETHERCAT_PROTOCOL);
   ll.sll_ifindex = ifindex;

   if( bind( fd, (const sockaddr *)&ll, sizeof(ll) ) < 0 )
   {
      cml.Error( "Error binding to ethernet hardware: %s\n", strerror(errno) );
      Close();
      return &EtherCatError::OpenHardware;
   }

   return 0;
}

const Error *LinuxEcatHardware::Close( void )
{
   if( fd >= 0 )
      close(fd);
   fd = -1;
   return 0;
}

const Error *LinuxEcatHardware::SendPacket( uchar *msg, uint16 len )
{
   CML_ASSERT( msg != 0 );
   CML_ASSERT( len > 14 );

   if( fd < 0 ) return &EtherCatError::EcatNotInit;

   struct sockaddr_ll ll;
   memset( &ll, 0, sizeof(ll) );

   ll.sll_ifindex = ifindex;
   ll.sll_family  = AF_PACKET;
   ll.sll_halen   = 6;
   memcpy( ll.sll_addr, msg, 6 );

   sendto( fd, msg, len, 0, (const sockaddr *)&ll, sizeof(ll) );
   return 0;
}

const Error *LinuxEcatHardware::RecvPacket( uchar *msg, uint16 *len, Timeout to )
{
   int32 timeout = (int32)to;
   int flags = 0;

   if( fd < 0 ) return &EtherCatError::EcatNotInit;

   if( timeout == 0 )
      flags |= MSG_DONTWAIT;

   else
   {
      struct timeval tv;

      tv.tv_sec = 0;
      if( timeout < 0 )
         tv.tv_usec = 0;
      else if( timeout < 1000 )
         tv.tv_usec = 1000 * timeout;
      else
      {
         tv.tv_sec = timeout / 1000;
         tv.tv_usec = 1000 * (timeout % 1000);
      }

      if( setsockopt( fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv) ) < 0 )
      {
         cml.Error( "Error setting timeout for ethernet socket read: %s\n", strerror(errno) );
         return &EtherCatError::ReadHardware;
      }
   }

   int ret = recv( fd, msg, *len, flags );

   if( ret < 0 )
   {
      if( errno == EAGAIN )
         return &ThreadError::Timeout;
      cml.Error( "Error reading from ethernet socket: %s\n", strerror(errno) );
      return &EtherCatError::ReadHardware;
   }

   *len = ret;
   return 0;
}

