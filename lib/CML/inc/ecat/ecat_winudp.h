#ifndef _DEF_INC_ECAT_WINUDP
#define _DEF_INC_ECAT_WINUDP

#include "CML_EtherCAT.h"

CML_NAMESPACE_START()

/**
 * This class provides an interface to the Ethernet ports on a windows
 * system.  It can be used to send and received formatted EtherCAT 
 * packets.
 *
 * Windows doesn't allow raw Ethernet packets to be sent, so this class
 * packages the EtherCAT packets into a UDP wrapper.
 */
class WinUdpEcatHardware: public EtherCatHardware
{
   uint32 hndl;
   uint32 recv;
   uint32 bcastip;
   char *ifname;
public:
   WinUdpEcatHardware( const char *name=0 );
   virtual ~WinUdpEcatHardware( void );
   const Error *Open( void );
   const Error *Close( void );
   const Error *SendPacket( uchar *msg, uint16 len );
   const Error *RecvPacket( uchar *msg, uint16 *len, Timeout timeout=-1 );
};

CML_NAMESPACE_END()

#endif
