
#ifndef _DEF_INC_ECAT_LINUX
#define _DEF_INC_ECAT_LINUX

#include "CML_EtherCAT.h"

CML_NAMESPACE_START()

/**
 * This class provides an interface to the Ethernet ports on a linux
 * system.  It can be used to send and received formatted EtherCAT 
 * packets.
 */
class LinuxEcatHardware: public EtherCatHardware
{
   int fd, ifindex;
   char *ifname;
public:
   LinuxEcatHardware( const char *name=0 );
   virtual ~LinuxEcatHardware( void );
   const Error *Open( void );
   const Error *Close( void );
   const Error *SendPacket( uchar *msg, uint16 len );
   const Error *RecvPacket( uchar *msg, uint16 *len, Timeout timeout=-1 );
};

CML_NAMESPACE_END()

#endif
