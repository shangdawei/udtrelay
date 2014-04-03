#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

/**
 * Simple socks4 wrapper for connect()
 */

typedef enum {PENDING, DONE, ERROR, ABSENT} status_t; 

int         init();
struct sockaddr *  getserver ();

const char * config_std = "/etc/minisocks.conf";

const char * conf_env_name = "MINISOCKS_CONFIG";
char * config_sav = NULL;
status_t conf_status = PENDING; 

const char * serv_env_name = "MINISOCKS_SERVER";
char * server_sav = NULL;
status_t serv_status = PENDING;

char * default_port = "1080"; 

char * config;
time_t checktime = 0;

struct addrinfo * server;

int init () {
    struct stat sbuf;
    FILE * f;
    
    if(0) {
        if ((config = getenv(conf_env_name)) == NULL) {
            config = strdup(config_std);
        }
        if(stat(config, &sbuf))
            return -1;
        if (checktime == sbuf.st_mtime && ! strcmp(config, config_sav))
            return 0;
        
        if((f = fopen(config,"r")) == NULL)
            return -1;
        
        fclose(f);
    }
    
    if (serv_status == PENDING) {
        char * serv_env_val;
        if (serv_env_val = getenv(serv_env_name)) {

            char * host = (char*) malloc(strlen(serv_env_val)+1);
            char * port = (char*) malloc(strlen(serv_env_val)+1);

            int n;
            if (1 <= (n = sscanf(serv_env_val, "%[^:]:%[^:]", host, port))){
                if(1 == n) // n == 1: port not specified - use default
                    port = default_port;
                
                struct addrinfo hints;
                
                memset(&hints, 0, sizeof(hints));
                hints.ai_flags    = AI_PASSIVE;
                hints.ai_family   = AF_INET;
                hints.ai_socktype = SOCK_STREAM; 

                //std::cout << host << ":" << port << std::endl;
                int rc;
                if((rc = getaddrinfo(host, port, &hints, &server)) == 0) {
                    server_sav = serv_env_val;
                    serv_status = DONE;
                }
                else {
                    //std::cout << "getaddrinfo = " <<  gai_strerror(rc) << std::endl;
                    serv_status = ERROR;
                }
            }
        }
        else {
            serv_status = ABSENT;
        }
    }
    
    //std::cout << " serv_status = " << serv_status << " DONE=" << ((status_t)DONE) << std::endl;
    return 0;
}

struct sockaddr * getserver () {
    init();
    if (serv_status == DONE)
        return server->ai_addr;
    else 
        return NULL;
}


int connect (int sd,  const struct sockaddr *name, socklen_t namelen) {
    struct sock_pkt_0 pkt0;
    int rc;
    typedef int (*Func_connect)(int,  const struct sockaddr *, socklen_t);
    
    struct addrinfo *s,h;
    
    Func_connect orig_connect = (Func_connect) dlsym(RTLD_NEXT,"connect");
    
    pkt0.pkt.vn = 4;
    pkt0.pkt.cd = 1;
    pkt0.null = '\0';
    
    if(name->sa_family == AF_INET) {
        memcpy(pkt0.pkt.dstport, &((struct sockaddr_in*) name)->sin_port,
                sizeof(pkt0.pkt.dstport));
        memcpy(pkt0.pkt.dstip, &((struct sockaddr_in*) name)->sin_addr.s_addr,
                sizeof(pkt0.pkt.dstip));
    }
    else if (name->sa_family == AF_INET6)  {
        errno = EAFNOSUPPORT;
        return -1;
    }
    else {
        errno = EAFNOSUPPORT;
        return -1;
    }
    
    struct sockaddr * srv;
    
    if (NULL == (srv = getserver())) {
        errno = EFAULT;
        return -1;
    }
    
    //std::cout << "OK !!!" << std::endl;
    
    if (0 != (rc = orig_connect(sd, srv, namelen)))
        return rc;

    //std::cout << "OK1 !!!" << std::endl;
    
    if(sizeof(pkt0) != (rc = send(sd, &pkt0, sizeof(pkt0),0))) {
        if (rc>=0)
            errno = 0; // we don't know why not all bytes was sent 
                       // to clear socket at once.
        return -1;
    }
    
    //std::cout << "OK2 !!!" << std::endl;
    
    if(sizeof(pkt0.pkt) != (rc = recv(sd, &pkt0.pkt, sizeof(pkt0.pkt),0))) {
        if (rc>=0)
            errno = 0; // we don't know why not all bytes was received 
                       // from clear socket at once.
        return -1;
    }
    
    if (pkt0.pkt.cd != 90) {
        shutdown(sd, SHUT_RDWR);
        errno = ECONNREFUSED;
        return -1;
    }
    return 0; 
}
