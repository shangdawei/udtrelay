#ifndef _UTILS_H_
#define _UTILS_H_

#include <sys/types.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstdio>
#include <arpa/inet.h>
#include <sys/socket.h>
//#include <sys/sysctl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <udt.h>

#if HAVE_GETIFADDRS
#    include <ifaddrs.h>
#else
#    include <getifaddrs.h>
#endif

#define DEBUG 1
#ifdef DEBUG
#  define __DEBUG__(x) std::cerr << __FILE__ << ":" << __LINE__ << ":  \tdebug: " <<  x << std::endl;
#else
#  define __DEBUG__(x)
#endif // DEBUG

#define __CROAK__(x) std::cerr << __FILE__ << ":" << __LINE__ << ": " << (x) << std::endl; std::exit(1);


namespace utl {

    using namespace std;

    string get_string (stringstream& is);
    int get_int (istream is);
    string i2str(int i);
    /*
     resolv service name and return port number in host order
     */
    in_port_t resolvService (string srv);
    /*
     Validate network address and return address family
     */
    int resolvAddress (string addr);

    char* dump_inetaddr(struct ::sockaddr_in * iaddr, char * buf, bool port=0);
    char* dump_str(char* str, char* buf, int sz1, int sz2, int sz3);
    
    bool sockaddr_match(sockaddr* addr1, sockaddr* addr2, bool portcmp = false);
    void sockaddr_mask(sockaddr* dst, sockaddr* src);
    bool check_source(sockaddr * addr, bool subnet);
    void memand (void * dst, void * src, size_t size);

    int worker(int sd1, int sd2, timeval * timeout = NULL);
    int worker(int fd_in, int fd_out, int sd, timeval * timeout = NULL);
    
    inline int timeval2utime(const timeval * t) {
        return t->tv_sec*1000000+t->tv_usec;
    }
    inline timeval * utime2timeval(timeval * t1, time_t t2) {
        t1->tv_sec = t2 / 1000000;
        t1->tv_usec = t2 % 1000000;
        return t1; 
    }
    std::vector<std::string> split (const std::string &inString, 
                                      const std::string &separator);
    std::vector<std::string> split (const std::string &inString, 
                                      const char * const separator);

}
#endif

