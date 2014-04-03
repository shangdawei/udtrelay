#ifndef SOCK_API_H_
#define SOCK_API_H_

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <udt.h>
#include <set>

namespace SOCK_API
{
    typedef enum{TCP, UDT} _proto_t;
    const int UDT_FD_BASE = 0xFFFFFF;

    class FDSET {
    public:
        ::fd_set 	fdset;
        ::UDT::UDSET	udset;

        FDSET ();
        ~FDSET ();
        void ZERO();
        void SET(UDTSOCKET sd);
        void CLR(UDTSOCKET sd);
        bool ISSET(UDTSOCKET sd);
    };
    
    inline _proto_t  getsocktype(UDTSOCKET sd) {
        return (sd < UDT_FD_BASE) ? TCP : UDT;
    }
    inline UDTSOCKET getsockfd(UDTSOCKET sd) {
        return (sd < UDT_FD_BASE) ? sd : sd-UDT_FD_BASE;
    }
    inline UDTSOCKET embedsockfd(UDTSOCKET sd, _proto_t proto) {
        return proto == UDT ? sd + UDT_FD_BASE : sd;
    }
    

    UDTSOCKET socket(int domain, int type, _proto_t proto);

    int bind    (UDTSOCKET sd, sockaddr* addr, socklen_t socklen);

    int connect (UDTSOCKET sd, sockaddr* addr, socklen_t socklen);

    int listen  (UDTSOCKET sd, int n);

    int accept  (UDTSOCKET sd, sockaddr* addr, socklen_t* socklenp);

    int send    (UDTSOCKET sd, void *buff, size_t n, int flags);

    int recv    (UDTSOCKET sd, void *buff, size_t n, int flags);
    
    int read     (UDTSOCKET sd, void *buff, size_t n, int timeout=0);
    int readn    (UDTSOCKET sd, void *buff, size_t n, int timeout=0);

    int write    (UDTSOCKET sd, void *buff, size_t n, int timeout=0);
    int writen   (UDTSOCKET sd, void *buff, size_t n, int timeout=0);


    int recvmsg (UDTSOCKET sd, msghdr *message, int flags);
    int recvmsg (UDTSOCKET sd, const char *buff, size_t n);

    int sendmsg (UDTSOCKET sd, msghdr *message, int flags);
    int sendmsg (UDTSOCKET sd, const char *buff, size_t n);

    int select  (int nfds, FDSET* rfds, FDSET* wfds, FDSET* efds, const struct timeval* timeout);

    int close(UDTSOCKET sd);
    int shutdown(UDTSOCKET sd,int how);

    int setsockopt(UDTSOCKET sd, int level, ::UDT::SOCKOPT optname, void* optval, socklen_t optlen);
    int getsockopt(UDTSOCKET sd, int level, ::UDT::SOCKOPT optname, void* optval, socklen_t* optlenp);
    
    int maxfdn(UDTSOCKET sd1, UDTSOCKET sd2);
    int maxfdn(std::set<UDTSOCKET> sd_set);
};

#endif /*SOCK_API_H_*/
