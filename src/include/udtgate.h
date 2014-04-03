#ifndef _UDTGATE_H
#define _UDTGATE_H

#ifdef  DEFAULT_INCLUDES
#  ifndef Win32
#    include <errno.h>
#    include <config.h>
#    include <unistd.h>
#    include <sys/socket.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <arpa/inet.h>
#    include <signal.h>
#    include <poll.h>
#    include <sys/types.h>
// #    include <sys/sysctl.h>
#    include <syslog.h>
#  else // Win32
#    include <winsock2.h>
#    include <Ws2tcpip.h>
#  endif
#
#  include <cstdlib>
#  include <cstring>
#  include <iostream>
#  include <errno.h>
#  include <assert.h>
#  include <stdarg.h>
#  include <udt.h>
#  include <logger.h>
#endif  // COMMON_INCLUDE

#include <udt.h>
#include <map>
#include <set>

const int MAX_NAME_SZ = 128;    // buffer size for addr/port/other names
const int BLOCK_SIZE  = 1024*1024; // buffer size for IO operations

struct cargs_t {
    UDTSOCKET udtsock;
    int       tcpsock;
    int       shutdown;
};

#define ACL_ENTRY_DST 0

class acl_entry {
public:
    static const uint32_t LOCAL_TOKEN    = 0xFFFFFFFF;
    static const uint32_t ATTACHED_TOKEN = 0xFFFFFFFE;

    in_addr_t src_addr;
    in_addr_t src_mask;
#if ACL_ENTRY_DST
    in_addr_t dst_addr;
    in_addr_t dst_mask;
#endif    
    uint16_t min_port;
    uint16_t max_port;
    
    acl_entry() :
        src_addr(0), src_mask(0),
#if ACL_ENTRY_DST
        dst_addr(0), dst_mask(0),
#endif
        min_port(0), max_port(0xFFFF)
    {
        
    }
    
};

class acl_table {
    std::set<acl_entry> table;
public:
    void clear();
    bool add(std::string entry);
    bool match(sockaddr_in * dst);
};

//typedef std::set<acl_entry_t> acl_table_t;

class globals {
public:
    static int  net_access; // network access 0 - loopback; 1 - local subnets; 2+ - any.
    static int  debug_level;
    static int  dump_message;
    static bool is_track_connections;
    static bool is_rendezvous;
    static bool is_demonize;
    static bool is_custom_acl;
    
    static acl_table custom_acl;
    
#ifdef UDP_BASEPORT_OPTION
    static int baseport;
    static int maxport;
#endif
    static char * sock_ident;
    static char * peer_ident;

    static char * app_ident;
    static char * serv_ident;

};

#pragma pack(1)
struct sock_pkt {
    char vn;
    char cd;
    char dstport[2];
    char dstip[4];
};
struct sock_pkt_0 {
    struct sock_pkt pkt;
    char null;
};
#pragma pack()


#endif
