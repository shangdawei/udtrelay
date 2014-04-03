#include <config.h>
#include <unistd.h>
#include <logger.h>
#include <netinet/in.h>
#include <utils.h>
#include <errno.h>
#include <string.h>

extern Logger logger;
extern int debug_level;


namespace utl {
    using namespace std;
    char* dump_inetaddr(sockaddr_in * iaddr, char * buf, bool port) {
    	if (port)
    	    sprintf(buf,"%s:%d",inet_ntoa(iaddr->sin_addr),
                ntohs(iaddr->sin_port));
    	else
    	    sprintf(buf,"%s",inet_ntoa(iaddr->sin_addr));

        return buf;
    }
    char * dump_str(char * str, char * buf, int buf_sz, int rcv_sz, int prn_sz) {
        int sz;
        // get minimal
        sz = buf_sz < rcv_sz ? buf_sz : rcv_sz;
        sz = sz  < prn_sz ? sz  : prn_sz;
        int p = 0; // 
        for (int i = 0; i < sz; i++) {
            char c = buf[i];
            if (c >= ' ' and c <= '~') {
                *(str+(p++)) = c;
            }
            else
                p += sprintf(str+p,"<%03d>", c);
        }
        if (prn_sz < rcv_sz) {
            p += sprintf(str+p,"...");
        }
        str[p] = '\0';
        return str;
    }

    string get_string (stringstream& is) {
        string s; is >> s; return s;
    }
    int get_int (istream is) {
        int i;  is >> i; return i;
    }
    string i2str(int i) {
        stringstream ss("");
        ss << i;
        return ss.str();
    }
    ////
     // resolv service name and return port number in host order
     ///
    in_port_t resolvService (string srv) {
        in_port_t port;
        if (port = atoi(srv.c_str()))
            return port;
        else {
            struct servent* sent;
            if (sent = getservbyname(srv.c_str(),  "udp")) {
                return ntohs(sent->s_port);
            }
        }
        throw(string("wrong service or port:")+srv);
    }
    ////
     // Validate network address and return address family
     ///
    int resolvAddress (string addr) {
        // __CROAK__("not implmented yet");
        return 1;
    }

    ////
     //Check if the address match local interfaces or subnets
     ///

    bool check_source (sockaddr * addr, bool subnet) {
//#if HAVE_GETIFADDRS

        char addr_str[256];
        char * where = "check_source:";
        int af;

        af = addr->sa_family;

        dump_inetaddr((sockaddr_in*) addr, addr_str);

        struct ifaddrs *ifp, *ifap;
        if(::getifaddrs(&ifap)<0)
            logger.log_die("getifaddrs failed: errno = %d\n", errno);
        //printf("getifaddrs = %d\n", ifap);

        for(ifp = ifap; ifp != NULL; ifp = ifp->ifa_next) {
            struct sockaddr addr1;

            //printf("matching against = %s\n", ifp->ifa_name);

            if (ifp->ifa_addr == NULL or ifp->ifa_netmask == NULL)
                continue;

            memcpy(&addr1, addr, sizeof(addr1));

            if(af != ifp->ifa_addr->sa_family)
                continue;

            if (subnet) {
                sockaddr_mask(&addr1,ifp->ifa_netmask);
                sockaddr_mask(ifp->ifa_addr,ifp->ifa_netmask);
            }

            if (logger.getDebugLevel()) {
                char addr1_str[256], ifaddr_str[256];
                dump_inetaddr((sockaddr_in*) &addr1, addr1_str);
                dump_inetaddr((sockaddr_in*) ifp->ifa_addr, ifaddr_str);

                logger.log_debug("%s matching %s against %s -> %s\n",where, addr1_str, ifp->ifa_name, ifaddr_str);
            }
            if(sockaddr_match(&addr1,ifp->ifa_addr)) {
            	logger.log_debug("%s connecting %s matched against %s\n",
            			where, addr_str,ifp->ifa_name);
                ::freeifaddrs(ifap);
                return true;
            }
        }
        logger.log_debug("%s no match\n", where);

        ::freeifaddrs(ifap);
        return false;
//#else // HAVE_GETIFADDRS
//        return true;
//#endif // HAVE_GETIFADDRS
    }

    bool sockaddr_match(sockaddr * addr1, sockaddr * addr2, bool portcmp) {

        assert(addr1->sa_family == addr2->sa_family
               && (addr1->sa_family == AF_INET || addr1->sa_family == AF_INET6));

        switch(addr1->sa_family) {
        case AF_INET:
            if (portcmp and ((sockaddr_in*) addr1)->sin_port != ((sockaddr_in*) addr1)->sin_port)
                return false;
            return memcmp(
                          & reinterpret_cast<sockaddr_in*>(addr1)->sin_addr,
                          & reinterpret_cast<sockaddr_in*>(addr2)->sin_addr,
                          sizeof (reinterpret_cast<sockaddr_in*>(addr1)->sin_addr)
                         ) ? false : true;
        case AF_INET6:
            if (portcmp and ((sockaddr_in6*) addr1)->sin6_port != ((sockaddr_in6*) addr1)->sin6_port)
                return false;
            return memcmp(
                          & reinterpret_cast<sockaddr_in6*>(addr1)->sin6_addr,
                          & reinterpret_cast<sockaddr_in6*>(addr2)->sin6_addr,
                          sizeof (reinterpret_cast<sockaddr_in6*>(addr1)->sin6_addr)
                         ) ? false : true;
        }
        return false;
    }
    void sockaddr_mask(sockaddr * addr1, sockaddr * netmask) {

        assert(addr1->sa_family == netmask->sa_family
               && (addr1->sa_family == AF_INET || addr1->sa_family == AF_INET6));

        switch(addr1->sa_family) {
        case AF_INET:
            memand(
                   & reinterpret_cast<sockaddr_in*>(addr1)->sin_addr,
                   & reinterpret_cast<sockaddr_in*>(netmask)->sin_addr,
                   sizeof (reinterpret_cast<sockaddr_in*>(addr1)->sin_addr)
                  );
            return;
        case AF_INET6:
            memand(
                   & reinterpret_cast<sockaddr_in6*>(addr1)->sin6_addr,
                   & reinterpret_cast<sockaddr_in6*>(netmask)->sin6_addr,
                   sizeof (reinterpret_cast<sockaddr_in6*>(addr1)->sin6_addr)
                  );
            return;
        }
    }
    void memand (void * dst, void * src, size_t size) {
    	char * dst1, *src1;

        for(dst1 = (char*) dst, src1 = (char*) src; size>0; size--, dst1++, src1++)
            *dst1 &= *src1;
    }
    
    std::vector<std::string> split (const std::string &inString, 
                                      const std::string &separator) {
        
       std::vector<std::string> returnVector; 
       std::string::size_type start = 0; 
       std::string::size_type end = 0; 

       while ((end=inString.find (separator, start)) != std::string::npos) 
       { 
          returnVector.push_back (inString.substr (start, end-start)); 
          start = end+separator.size(); 
       } 

       returnVector.push_back (inString.substr (start)); 

       return returnVector; 

    }
    std::vector<std::string> split (const std::string &inString, 
                                      const char * const separator) {
       return split(inString, std::string(separator)); 
    }
}
