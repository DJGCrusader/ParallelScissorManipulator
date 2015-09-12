#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "CML.h"
#include "ecat_winudp.h"

CML_NAMESPACE_USE();

#define MAX_INTERFACE        16
#define ETHERCAT_PROTOCOL    0x88A4
#define RCVALL_ON            1
#define SIO_RCVALL           _WSAIOW(IOC_VENDOR,1)

// local functions
static int inet_aton( const char *name, struct in_addr *addr );

/**
   Create an EtherCAT hardware interface which uses UDP formatted messages.  This is the only type of EtherCAT interface that can
   be used under Windows without installing special drivers.

   The low level EtherCAT protocol normally does not use an IP address, however since this driver transmits EtherCAT packets over 
   UDP/IP, the Ethernet interface used with this driver must have a valid IP address assigned.  In addition, the network mask 
   associated with the Ethernet interface should be defined in such a way that no other network interface on the same PC is a member
   of the same network.  That is, if multiple interfaces are installed then they should be allocated to seperate networks.

   i.e. ( IP1 & mask1 ) != (IP2 & mask2) 
      where IP1 and mask1 are the IP address and net mask of the first interface, and IP2 and mask2 are for the second interface.

   For example, the following two interfaces are on different networks:
      IP: 192.168.1.1   mask: 255.255.255.0
      IP: 192.168.2.1   mask: 255.255.255.0

   but the following two interfaces are on the same network:
      IP: 192.168.1.1   mask: 255.255.255.0
      IP: 192.168.1.2   mask: 255.255.255.0

   This is important because this drive has no direct control of which interface the packets are being sent out.  This is entirely
   controlled by the upper layer routing algorithms in the windows network stack.

   The name parameter passed to this function can be used to identify which interface this object should bind to.  It can take 
   any of the following forms:

   - If not specified, then the first valid interface found will be used.  This is useful if there's only one interface on the PC.

   - If of the form; eth0, eth1, eth2, etc, then the nth valid interface will be used.

   - For more control, the IP address of the desired interface can be passed.  This should be sent as a string in dotted decimal
     notation.  For example: "192.168.1.1"

   @param name Used to identify the Ethernet interface as described above.
*/
WinUdpEcatHardware::WinUdpEcatHardware( const char *name )
{
   hndl = 0;
   recv = 0;
   if( !name )
      ifname = 0;
   else
      ifname = CloneString( name );
   SetRefName( "UdpEcatHw" );
}

WinUdpEcatHardware::~WinUdpEcatHardware( void )
{
   delete ifname;
   KillRef();
   Close();
}

const Error *WinUdpEcatHardware::Open( void )
{
   // If the interface is already open, just return success
   if( recv ) return 0;

   // Initialize Winsock
   WSADATA wsaData;
   int err = WSAStartup(MAKEWORD(2,2), &wsaData);
   if( err != 0 )
   {
      cml.Error( "WINUDP: Error returned from WSAStartup: %d\n", err );
      return &EtherCatError::OpenHardware;
   }

   // Create a UDP socket
   hndl = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
   if( hndl == INVALID_SOCKET )
   {
      hndl = 0;
      cml.Error( "WINUDP: Unable to create UDP socket %d\n", WSAGetLastError() );
      return &EtherCatError::OpenHardware;
   }

   // Get a list of the IP addresses of all network adapters found in the system
   INTERFACE_INFO info[ MAX_INTERFACE ];
   DWORD ct;
   if( WSAIoctl( hndl, SIO_GET_INTERFACE_LIST, 0, 0, &info, sizeof(info), &ct, 0, 0 ) )
   {
      cml.Error( "WINUDP: Error %d returned when trying to read interface list.\n", WSAGetLastError() );
      Close();
      return &EtherCatError::OpenHardware;
   }
   ct /= sizeof(info[0]);

   if( !ct )
   {
      cml.Error( "WINUDP: No Ethernet interfaces found on system.  Quitting\n" );
      Close();
      return &EtherCatError::OpenHardware;
   }

   // Output the list to the logger for debugging.
   uint32 ipAddr[ MAX_INTERFACE ];
   uint32 ipMask[ MAX_INTERFACE ];
   for( DWORD i=0; i<ct; i++ )
   {
      struct sockaddr_in *ptr;

      ptr = (struct sockaddr_in *)&info[i].iiAddress;
      ipAddr[i] = ptr->sin_addr.s_addr;
      char addr[20];
      strncpy( addr, inet_ntoa(ptr->sin_addr), sizeof(addr) );

      ptr = (struct sockaddr_in *)&info[i].iiNetmask;
      ipMask[i] = ptr->sin_addr.s_addr;
      char mask[20];
      strncpy( mask, inet_ntoa(ptr->sin_addr), sizeof(mask) );

      cml.Debug( "WINUDP:  Interface %d  addr: %s, netmask: %s, flags: 0x%04x\n", i, addr, mask, info[i].iiFlags );
   }

   // Select the interface to use based on the name passed when the object was created.
   struct in_addr passedAddr;
   int num = -1;

   // If no name was passed, just use the first interface found
   if( !ifname )
      num = 0;

   // If a valid IP address was passed, look for it.
   else if( inet_aton( ifname, &passedAddr ) > 0 )
   {
      for( DWORD i=0; i<ct; i++ )
      {
         if( passedAddr.s_addr == ipAddr[i] )
            num = i;
      }
      if( num < 0 )
      {
         cml.Error( "WINUPD: Passed IP address %s could not be found in system.  Init fail\n", ifname );
         Close();
         return &EtherCatError::OpenHardware;
      }
   }

   // Check for an interface specified by number
   else if( sscanf( ifname, "eth%d", &num ) != 1 )
   {
      cml.Error( "WINUPD: Unable to parse interface name: %s\n", ifname );
      Close();
      return &EtherCatError::OpenHardware;
   }

   if( (num < 0) || (num >= (int)ct) )
   {
      cml.Error( "WINUPD: Couldn't find interface named: %s\n", ifname );
      Close();
      return &EtherCatError::OpenHardware;
   }

   passedAddr.s_addr = ipAddr[num];
   cml.Debug( "WINUDP: Passed interface name %s selected IP address %s\n", (ifname ? ifname : "None" ), inet_ntoa(passedAddr) );

   // Find the broadcast address for the network associated with this interface.
   bcastip = ipAddr[num] | ~ipMask[num];
   passedAddr.s_addr = bcastip;

   // Set an option to allow me to send broadcast packets
   int yes=1;
   if( setsockopt( hndl, SOL_SOCKET, SO_BROADCAST, (char *)&yes, sizeof(yes) ) != 0 )
   {
      cml.Error( "WINUDP: Error setting broadcast option on socket: %d\n", WSAGetLastError() );
      Close();
      return &EtherCatError::OpenHardware;
   }

   // Create a second socket for receiving
   recv = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
   if( recv == INVALID_SOCKET )
   {
      recv = 0;
      cml.Error( "WINUDP: Unable to create receive socket %d\n", WSAGetLastError() );
      Close();
      return &EtherCatError::OpenHardware;
   }

   // Bind this socket 
   sockaddr_in addr;
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = INADDR_ANY;
   addr.sin_port = htons(ETHERCAT_PROTOCOL);

   err = bind( recv, (sockaddr *)&addr, sizeof(addr) );
   if( err == SOCKET_ERROR )
   {
      cml.Error( "WINUDP: Unable to bind socket %d\n", WSAGetLastError() );
      Close();
      return &EtherCatError::OpenHardware;
   }

   return 0;
}

const Error *WinUdpEcatHardware::Close( void )
{
   if( recv || hndl )
   {
      cml.Debug( "WINUDP: Closing windows socket\n" );
      if (recv) closesocket(recv);
      if (hndl) closesocket(hndl);
      recv = 0;
      hndl = 0;
   }
   return 0;
}

const Error *WinUdpEcatHardware::SendPacket( uchar *msg, uint16 len )
{
   CML_ASSERT( msg != 0 );
   CML_ASSERT( len > 14 );

   if( !hndl ) return &EtherCatError::EcatNotInit;

   // Due to serious limitations in windows, I need to send the EtherCAT 
   // frame formatted as a UDP message.  This will require me to strip out
   // the 14 byte Ethernet header and allow windows to add it's own 
   // headers to the message.
   //
   // I also need to fix the message padding.  If the length of the message
   // being sent to me is 60 bytes then it's probably been padded.  
   // I'll reduce the amount of padding so that it works out for a UDP style
   // message.
   if( len == 60 )
   {
      int16 hdr = bytes_to_int16( &msg[14] );
      int16 l = hdr & 0x07ff;

      // The EtherCAT frame length, including the 2 byte header is len+2.
      // The UDP header is 42 bytes long, so this needs to be at least 18
      // bytes including any padding.  I also add 14 to this for the Ethernet
      // header which will be removed below.
      len = l+16;
      if( len < 32 )
         len = 32;
   }

   sockaddr_in addr;
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = bcastip; // INADDR_BROADCAST;
   addr.sin_port = htons(ETHERCAT_PROTOCOL);

   int ct = sendto( (SOCKET)hndl, (char*)&msg[14], len-14, 0, (sockaddr*)&addr, sizeof(addr) );
   if( ct == SOCKET_ERROR )
   {
      cml.Error( "WINUDP: Error sending to socket %d\n", WSAGetLastError() );
      return &EtherCatError::WriteHardware;
   }

   return 0;
}

extern void CheckWindowsThreadStop( void );
const Error *WinUdpEcatHardware::RecvPacket( uchar *msg, uint16 *len, Timeout timeout )
{
   if( !hndl ) return &EtherCatError::EcatNotInit;

   CML_ASSERT( msg != 0 );
   CML_ASSERT( *len > 14 );

   // Wait for data to be available on my socket with a timeout.
   int32 remain = (int32)timeout;
   while( 1 )
   {
      // The max I'll wait in any call is 20ms.  This allows me to respond
      // quickly if the thread is stopped
      int ms = ((remain>=0) && (remain<=20)) ? remain : 20;
      if( remain > 0 ) remain -= ms;

      struct timeval tval;
      tval.tv_sec = 0;
      tval.tv_usec = ms * 1000;
      struct fd_set readSet = { 1, { (SOCKET)recv } };

      int ret = select( 1, &readSet, 0, &readSet, &tval );
      if( ret == SOCKET_ERROR )
      {
         cml.Error( "WINUDP: select error %d\n", WSAGetLastError() );
         return &EtherCatError::ReadHardware;
      }

      if( ret ) break;

      // Timeout.
      if( !remain )
         return &ThreadError::Timeout;

      CheckWindowsThreadStop();
   }

   sockaddr_in addr;
   int addrLen = sizeof(addr);
   char buf[1500];

   int ct = recvfrom( (SOCKET)recv, buf, sizeof(buf), 0, (sockaddr*)&addr, &addrLen );
   if( ct == SOCKET_ERROR )
   {
      cml.Error( "WINUDP: Error receiving socket: %d\n", WSAGetLastError() );
      return &EtherCatError::ReadHardware;
   }

   // The data I receive will start with the EtherCAT header.
   // Check to make sure this looks reasonable.  If not I just
   // return a blank frame which will be discarded by the upper
   // level EtherCAT process
   int16 hdr = bytes_to_int16( (uchar*)buf );
   int16 sz = (hdr & 0x07FF) + 2;

   if( (hdr & 0xF000) != 0x1000 || (sz != ct) )
   {
      *len = 0;
      return 0;
   }

   // Add padding if the frame length is less then (60-14)
   while( sz < 46 )
      buf[sz++] = 0;

   // The header looks reasonable, create a fake Ethernet header
   // and pad the message if necessary
   int i;
   for( i=0; i<12; i++ )
      msg[i] = 0xff;
   msg[12] = 0x88;
   msg[13] = 0xA4;

   if( *len < sz+14 ) sz = *len-14;
   for( i=0; i<sz; i++ )
      msg[i+14] = buf[i];

   *len = sz+14;
   return 0;
}

static int inet_aton( const char *name, struct in_addr *addr )
{
   int a, b, c, d;
   if( sscanf( name, "%d.%d.%d.%d", &a, &b, &c, &d ) != 4 )
      return 0;
   if( (a < 0) || (a>255) ) return 0;
   if( (b < 0) || (b>255) ) return 0;
   if( (c < 0) || (c>255) ) return 0;
   if( (d < 0) || (d>255) ) return 0;

   uint32 x = (a<<24) | (b<<16) | (c<<8) | d;
   addr->s_addr = htonl(x);
   return 1;
}

