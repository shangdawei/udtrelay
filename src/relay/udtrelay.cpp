#include <sys/wait.h>
#include <signal.h>
#include <udt.h>
#ifdef UDT_CCC_OPTION
	#include <cc.h>
#endif
#include <setproctitle.h>
#define  DEFAULT_INCLUDES
#include <udtgate.h>
#include <utils.h>
#include <stdlib.h>
#include <socket_api.h>
#include "udtrelay.h"

#include <map>

using namespace std;

void* start_child(void*);

/**
 *  Worker for traffering data from sock to peer sockets
 */
void* sock2peer_worker(void*);

/**
 *  Worker for traffering data from peer to sock sockets
 */
void* peer2sock_worker(void*);

void* worker(void*);


/**
 *  Set default options for UDT sockets
 */
void  setsockopt(UDTSOCKET);

bool  check_udp_buffer(UDTSOCKET,UDTOpt);

int   parse_udt_option (char * optstr);

bool  is_custom_value(UDTOpt optcode);

int udt_send_all (UDTSOCKET sock, char * data, int size);
int tcp_send_all (int sock, char * data, size_t size);

/**
 *  Check address against local interfaces/subnets
 *  returns true if match
 */

void exit_handler(int);

sockaddr_in   addr_any;

int PID;
char sPeer_addr[MAX_NAME_SZ+1] = "";
char sPeer_listen_port[MAX_NAME_SZ+1] = "";
char sPeer_remote_port[MAX_NAME_SZ+1] = "";
char sSocks_port[MAX_NAME_SZ+1] = "";

char sPeer_rzv_lport[MAX_NAME_SZ+1] = "";
char sPeer_rzv_rport[MAX_NAME_SZ+1] = "";

addrinfo hints;

addrinfo *pPeeraddr = NULL; // remote UDT peer address/port
addrinfo *pServaddr = NULL; // local  UDT peer address/port

addrinfo *pPeerRzvLocal  = NULL; // UDT client allocates it in rzv node
addrinfo *pPeerRzvRemote = NULL; // UDT server connects to it in rzv mode


enum {DUAL,SERVER,CLIENT} mode  = DUAL;

int rcvbuffer = BLOCK_SIZE;


typedef struct {
    char * optname;
    UDTOpt	   optcode;
} UDT_OPTION_T;

UDT_OPTION_T UDT_OPTIONS_LIST [] = {
    {"UDT_MSS",		UDT_MSS},
    {"UDT_RCVBUF",	UDT_RCVBUF},
    {"UDP_RCVBUF",	UDP_RCVBUF},
    {"UDT_SNDBUF",	UDT_SNDBUF},
    {"UDP_SNDBUF",	UDP_SNDBUF},
    {"UDT_LINGER",	UDT_LINGER}
};

typedef std::map<UDTOpt, int> UDT_OPTIONS_T;

UDT_OPTIONS_T udt_options;

#ifdef UDT_CCC_OPTION
char* cc_lib[] = {"UDT", "Vegas","TCP","ScalableTCP","HSTCP",
        "BiCTCP", "Westwood", "FAST"};
char* ccc = "";
#endif

extern char *optarg;
extern int   optind, opterr;


Logger logger("udtrelay");

int main(int argc, char* argv[], char* envp[])
{

    int c;
    static char optstring [] = "hdDNLCSP:R:B:c:U:X:A:";
    opterr=0;
    UDTSOCKET ludtsock;
    int       ltcpsock = 0;
    vector<string> custom_acl_entries;
    acl_table custom_acl_table;

    logger.setInteractive(true);
    
    globals::app_ident = "udtrelay";
    
    char *usage = \
        "\nusage:\n"
        "\n"
        "udtrelay [OPTIONS] <socks_port> <peer_port> <peer_addr[:port]>  \n"
        "\n"
        "  <socks_port>       TCP ports to listen for incomming client socks connections.\n"
        "  <peer_port>        UDT/UDP port to use for incomming peer UDT connections.\n"
        "                     The same peer port  number is used for outgoing UDT\n"
        "                     connections by default\n"
        "\n"
        "  <peer_addr[:port]> Remote UDT peer address and optional custom remote port.\n"
        "\n"
        "  OPTIONS: \n"
        "    -h               Show this help and exit.\n"
        "    -d               Encrease debug level, superset of -L.\n"
        "    -X <len>         dump fisrt <len> bytes as ASCII from each message.\n"
        "                     this option also sets maximal debug level.\n"
        "                     if debug level was set to 3 than default value of\n"
        "                     this option is 20\n"
        "    -D               Demonize.\n"
        "    -L               Log connections\n"
        "    -N               Allow connections from/to attached subnets \n"
        "                     (by default only connections from/to local device\n"
        "                     are permited);\n"
        "                     appling this option twice - allows all incoming\n"
        "                     and outgoing coonections.\n"
#ifdef UDT_ACL_OPTION        
        "    -A <acl>         (-) Setup custom access control list for incoming "
#endif        
        "                     connections, see README.udtrelay for details.\n"
        "    -C               Client-only mode: don't accept incoming peer/UDT\n"
        "                     connections.\n"
        "    -S               Server-only mode: don't accept outgoing socks\n"
        "                     connections.\n"
        "    -R <lp>[:<rp>]   Turn the rendezvous mode on.\n"
        "                     * UDT peer client allocates <lp> port and tries\n"
        "                       to connect to the servers's <peer_port>\n"
        "                     * UDT peer server allocates <peer_port> and tries\n"
        "                       to connect to the clinet's <rp> port which is \n"
        "                       by default equals to <lp>.\n"

#ifdef UDP_BASEPORT_OPTION
        "    -P <from:to>     UDP port range to use for UDT data chanel.\n"
#endif
#ifdef UDT_CCC_OPTION
        "    -c <ccc>         Congetion control class:\n"
        "                       UDT (default), TCP, Vegas, ScalableTCP, HSTCP,\n"
        "                       BiCTCP, Westwood, FAST.\n"
#endif
        "    -U <opt=val>     Set some additional UDT options for UDT socket:\n"
        "                       UDT_MSS, UDT_RCVBUF, UDT_SNDBUF, UDP_RCVBUF or\n"
        "                       UDP_SNDBUF.\n"
        "                     Quantifiers K(ilo) and M(ega) are accepted as \n"
        "                     suffixes.\n"
        "\n"
        "  Options marked with (-) have not been yet implemented."
        "\n";

    while ((c=getopt(argc, argv, optstring)) != -1) {
        switch (c) {
        case 'h':
            logger.log_info("%s", usage);
            exit(0);
        case 'N':
            globals::net_access++;
            break;
#ifdef UDT_ACL_OPTION
        case 'A':
            globals::is_custom_acl = true;
            custom_acl_entries = utl::split(string(optarg), "+"); 
            for(int i=0; i<custom_acl_entries.size();i++) {
                cout << custom_acl_entries[i].c_str() << endl;
                if (! custom_acl_table.add(custom_acl_entries[i])) {
                    logger.log_die("Error parse -A option value in %s\n",
                            custom_acl_entries[i].c_str());
                }
            }
            exit(0);
            break;
#endif            
        case 'd':
            globals::debug_level++;
            break;
        case 'X':
            globals::dump_message = atoi(optarg);
            if (globals::dump_message <= 0 or globals::dump_message > 10000)
                logger.log_die("Wrong -X option value\n%s", usage);
            break;
        case 'D':
            globals::is_demonize = true;
            break;
        case 'L':
            globals::is_track_connections = true;
            break;
        case 'C':
            if (mode != DUAL)
                logger.log_die(" -C option can not be used with -S.\n");
            mode = CLIENT;
            break;
        case 'S':
            if (mode != DUAL)
                logger.log_die(" -S option can not be used with -C.\n");
            mode = SERVER;
            break;
        case 'R':
            globals::is_rendezvous = true;
            int c;
            if((c = sscanf(optarg,"%[^:]:%[^:]",sPeer_rzv_lport,sPeer_rzv_rport)) < 1)
                logger.log_die("Wrong -R option syntax.\n");
            if (c = 1)
                strcpy(sPeer_rzv_rport,sPeer_rzv_lport);
            break;
#ifdef UDP_BASEPORT_OPTION
        case 'P':
            if(sscanf(optarg,"%d:%d",&globals::baseport, &globals::maxport) != 2)
                logger.log_die("Wrong -P option syntax.\n");
            if (globals::baseport < 1024 || globals::baseport > 0xFFFE ||
                    globals::maxport  < 1024 || globals::maxport  > 0xFFFE || 
                    globals::maxport <= globals::baseport)
                logger.log_die("Wrong -P option values: %s.\n", optarg);
            break;
#endif
#ifdef UDT_CCC_OPTION
        case 'c':
            ccc = (char*) malloc(strlen(optarg)+1);
            strcpy(ccc, optarg);
            {
                bool ccc_ok = false;
                for (int i=0; i< sizeof(cc_lib)/sizeof(*cc_lib); ++i ) {
                    if (! strcasecmp(cc_lib[i], ccc)) {
                        ccc_ok = true;
                        break;
                    };
                }
                if (not ccc_ok)
                    logger.log_die("\nUnsupported CC class (-c option): %s. Use -h for help\n",ccc);
            }
            break;
#endif            
        case 'U':
            parse_udt_option(optarg);
            break;
        case '?':
            logger.log_die("Unknown option: %s. Use -h for help\n", argv[optind-1]);
        default:
            break;
        }
    }

    globals::app_ident   = "udtrelay";
    globals::sock_ident =  "departing";
    globals::peer_ident  = "arriving";
    globals::serv_ident  = globals::app_ident;

    
    
    addr_any.sin_family = AF_INET;
    addr_any.sin_port = 0;
    addr_any.sin_addr.s_addr = INADDR_ANY;

    if (setpgid(getpid(),getpid()) <0 ) {
	perror("setgrp failed");
	exit(1);
    }
    

    if(globals::debug_level > 3)
        globals::debug_level = 3;
    if(globals::dump_message > 0)
        globals::debug_level = 3;
    if(globals::debug_level == 3 and globals::dump_message == 0)
        globals::dump_message = 20;

    logger.setDebugLevel(globals::debug_level);

    if ((0 == argc-optind))
        logger.log_die("\nudtrelay (%s, build on \"%s\" with UDT v%s)\n\n%s\n\n", 
                PACKAGE_STRING, __DATE__, "?.?", "Use -h for help");
    
    if ((3 > argc-optind))
        logger.log_die("missed arguments\n%s", usage);

    logger.setInteractive(false);

    fclose(stdin);
    
    strncpy(sSocks_port, argv[optind], MAX_NAME_SZ);
    strncpy(sPeer_listen_port, argv[optind+1], MAX_NAME_SZ);

    char fmt[256] = "%";
    sprintf(&fmt[strlen(fmt)],"%d",MAX_NAME_SZ);
    strcat(fmt,"[^:]:%");
    sprintf(&fmt[strlen(fmt)],"%d",MAX_NAME_SZ);
    strcat(fmt,"s");

    sscanf(argv[optind+2],fmt, sPeer_addr, sPeer_remote_port);

    if (!sPeer_remote_port[0])
        strncpy(sPeer_remote_port, sPeer_listen_port, MAX_NAME_SZ);
    
    //addrinfo *pServaddr;

    logger.log_debug(1, "peer addr = %s peer_rport = %s peer_lport = %s\n",
              sPeer_addr, sPeer_remote_port, sPeer_listen_port);
    
    if (globals::is_demonize) { 
       switch (fork()) {
       case 0:  /* Child (left running on it's own) */
          break;
       case -1:
          perror("fork failed");
          exit(1);
       default:  /* Parent */
          exit(0);
       }
       setsid();  /* Move into a new session */
       logger.syslogOn();
       fclose(stdout);
       fclose(stderr);
    }

    logger.log_info("");
    logger.log_info("Starting %s (%s).\n", globals::app_ident, PACKAGE_VERSION);
    
    do { // just block
    	int pid = 0;
    	int server_pid = 0;
    	int client_pid = 0;
    	
    	
    	if (mode == DUAL or mode == SERVER) { // udt->tcp
    		server_pid = fork(); 
                if (server_pid==-1) logger.log_die("% server fork failed.\n", globals::peer_ident);
			if (!server_pid) {
				mode = SERVER;
				globals::serv_ident = globals::peer_ident;
				break;
			}
    	}
    	if (mode == DUAL or mode == CLIENT) { // tcp->udt
    		client_pid = fork();
			if (client_pid==-1) logger.log_die("% server fork failed.\n", globals::sock_ident);
			if (!client_pid) {
				mode = CLIENT;
				globals::serv_ident = globals::sock_ident;
				break;
			}
    	}
 	
    	signal(SIGTERM, exit_handler);
    	signal(SIGINT, exit_handler);
    	signal(SIGQUIT, exit_handler);
    	
    	do {
    		setpgid(getpid(),getpid());
    		if((pid = wait(NULL)) > 0) break;
    	}
    	while (errno == EINTR);
    	
    	if (pid == server_pid)
    		logger.log_notice("%s server died. exiting.\n", globals::peer_ident);
    	if (pid == client_pid)
    		logger.log_notice("%s server died. exiting.\n", globals::sock_ident);
    	
    	exit_handler(SIGCHLD);

    } while (false);
    
	utl::initsetproctitle(argc, argv, envp);
	utl::setproctitle("%s: %s", globals::app_ident, globals::serv_ident);

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char * listen_port = mode == SERVER ? sPeer_listen_port : sSocks_port;

    if (0 != getaddrinfo(NULL, listen_port, &hints, &pServaddr))
    {
        logger.log_die("Illegal port number or port is busy.\n");
        return 1;
    }


    if (0 != getaddrinfo(sPeer_addr, sPeer_remote_port, &hints, &pPeeraddr))
    {
        logger.log_die("Incorrect peer network address.\n");
        return 1;
    }
    
    if (globals::is_rendezvous) {
        if (0 != getaddrinfo(NULL, sPeer_rzv_lport, &hints, &pPeerRzvLocal))
        {
            logger.log_die("Incorrect peer network address.\n");
            return 1;
        }
        if (0 != getaddrinfo(sPeer_addr, sPeer_rzv_rport, &hints, &pPeerRzvRemote))
        {
            logger.log_die("Incorrect peer network address.\n");
            return 1;
        }
    }

    
    if (mode == SERVER) {
        if(!globals::is_rendezvous) {
            ludtsock = UDT::socket(pServaddr->ai_family, pServaddr->ai_socktype, pServaddr->ai_protocol);
            setsockopt(ludtsock);
            if (UDT::ERROR == UDT::bind(ludtsock, pServaddr->ai_addr, pServaddr->ai_addrlen))
            {
                logger.log_err("udt bind: %s\n", UDT::getlasterror().getErrorMessage());
                return 0;
            }
    
            if (UDT::ERROR == UDT::listen(ludtsock, 10))
            {
                logger.log_err("udt listen: %s\n", UDT::getlasterror().getErrorMessage());
                return 0;
            }
            logger.log_info("accepting %s connections at port: %s\n", globals::peer_ident, sPeer_listen_port);
        }
    }
    else {
        int on = 1;
        ltcpsock = socket(pServaddr->ai_family, pServaddr->ai_socktype, pServaddr->ai_protocol);
        if (setsockopt(ltcpsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on))
            logger.log_die("setsockopt() error.");
        if (-1 == bind(ltcpsock, pServaddr->ai_addr, pServaddr->ai_addrlen))
        {
            logger.log_err("tcp bind: %s\n",strerror(errno));
            return 0;
        }
        if (listen(ltcpsock, 10))
        {
            logger.log_err("tcp listen error\n");
            return 0;
        }
        logger.log_info("accepting %s connections at port: %s\n", globals::sock_ident, sSocks_port);
    }
    //cout << "server is ready at port: " << service << endl;

    sockaddr_storage clientaddr;
    int addrlen = sizeof(clientaddr);


    while (true)
    {
        UDTSOCKET audtsock;
        int atcpsock;
        pthread_t childthread;
        char clienthost[NI_MAXHOST];
        char clientservice[NI_MAXSERV];


        if (mode == SERVER) {
            if (!globals::is_rendezvous) {
                if (UDT::INVALID_SOCK == (audtsock = UDT::accept(ludtsock, (sockaddr*)&clientaddr, &addrlen)))
                {
                    logger.log_err("accept: %s\n", UDT::getlasterror().getErrorMessage());
                    return 0;
                }
            } else {
                audtsock = UDT::socket(pServaddr->ai_family, pServaddr->ai_socktype, pServaddr->ai_protocol);
                
                setsockopt(audtsock);
                
                if (UDT::ERROR == UDT::bind(audtsock, pServaddr->ai_addr, pServaddr->ai_addrlen))
                {
                    logger.log_err("udt bind: %s\n", UDT::getlasterror().getErrorMessage());
                    return 0;
                }
                
                int rc;
                while((rc = UDT::connect(audtsock, pPeerRzvRemote->ai_addr, pPeeraddr->ai_addrlen)) < 0) {
                    if (rc < 5000) {
                        //logger.log_notice("cannot connect to peer: %s\n", 
                        //        UDT::getlasterror().getErrorMessage());
                        //sleep(1);
                    }
                    else {
                        logger.log_err("peer connection fatal error: %s", 
                                UDT::getlasterror().getErrorMessage());
                        return 0;
                    }
                }
                int len = sizeof(sockaddr);
                UDT::getpeername(audtsock, (sockaddr *) &clientaddr, &len);
            }
            
            getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
            
            if (not utl::sockaddr_match((sockaddr*)(&clientaddr), pPeeraddr->ai_addr)) {
                    close(audtsock);
                    logger.log_notice("rejected peer connection: %s:%s\n", clienthost, clientservice);
                    continue;
            }
            if (globals::debug_level or globals::is_track_connections)
            	logger.log_notice("accepted peer connection: %s:%s\n", clienthost, clientservice);

            setsockopt(audtsock);
            pthread_create(&childthread, NULL, start_child,  &audtsock);
        }
        else {
            if (-1 == (atcpsock = accept(ltcpsock, (sockaddr*)&clientaddr, (socklen_t*) &addrlen)))
            {
                logger.log_err("accept: error: %s\n", strerror(errno));
                return 0;
            }
            getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
            
            if (globals::net_access < 2) { // not -N -N options
            	bool subnet = globals::net_access > 0 ? true: false; // at least -N option - subnet mode
            	if ( not utl::check_source((sockaddr *)&clientaddr, subnet)) {
            		close(atcpsock);
            		logger.log_notice("rejected socks connection: %s:%s\n", clienthost, clientservice);
            		continue;
            	}
            }
            if (globals::debug_level or globals::is_track_connections)
            	logger.log_notice("accepted socks connection: %s:%s\n", clienthost, clientservice);
            pthread_create(&childthread, NULL, start_child,  &atcpsock);
        }
    }
    if (mode == SERVER)
        UDT::close(ludtsock);
    else
        close(ltcpsock);

    return 1;
}
void* start_child(void *servsock) {

    struct cargs_t cargs;
    pthread_t rcvthread, sndthread;
    struct sock_pkt spkt;
    char c;
    struct sockaddr clientaddr;
    int addrlen = sizeof(sockaddr);
    char clienthost[NI_MAXHOST];
    char clientservice[NI_MAXSERV];
    char * conntype;

    char buf[1024];

    cargs.shutdown = 0;

    if (mode == SERVER) {
        UDT::getpeername(*(UDTSOCKET*) servsock,&clientaddr, &addrlen);
        conntype = "UDT";
    }
    else {
        getpeername(*(int*) servsock, &clientaddr, (socklen_t*) &addrlen);
        conntype = "TCP";
    }
    getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
    
    
    while (true) {
        if (mode == SERVER) { // utd -> tcp
            int peersock = socket(AF_INET,SOCK_STREAM,0);

            cargs.udtsock = *(UDTSOCKET*) servsock;
            cargs.tcpsock = peersock;

            logger.log_debug(2, "udt accepted\n");
            
            // receive sizeof(spkt) bytes
            char * spktp = (char*) &spkt;
            for (char * p = spktp; p < spktp + sizeof(spkt);) {
            	p += UDT::recv(cargs.udtsock, p, spktp + sizeof(spkt) - p, 0);
            };

            logger.log_debug(2, "udt <- soks (-) recv\n");

            if(spkt.vn != 4) {
                logger.log_warning("wrong socks version (%d), connection closed\n", spkt.vn);
                spkt.cd = 91;
                UDT::send(cargs.udtsock, (char*) &spkt, sizeof(spkt), 0);
                return 0;
            }
            if(spkt.cd != 1) {
                logger.log_warning("! wrong command - only \"connect\" is supported\n");
                spkt.cd = 91;
                UDT::send(cargs.udtsock,(char*) &spkt, sizeof(spkt), 0);
                return 0;
            }
            
           
            do {
                UDT::recv(cargs.udtsock,&c, 1, 0);
            } while (c != 0);

            logger.log_debug(2, "udt <- soks (+) recv\n");


            sockaddr_in paddr;
            memset(&paddr,0,sizeof(paddr));

            paddr.sin_family = AF_INET;

            paddr.sin_port = *(short unsigned*) spkt.dstport;

            memcpy(&paddr.sin_addr, spkt.dstip, 4);
            
            if (globals::net_access < 2) { // not -N -N options
                bool subnet = globals::net_access > 0 ? true: false; // at least -N option - subnet mode
                if ( not utl::check_source((sockaddr *)&paddr, subnet)) {
                    char buf[50];
                    spkt.cd = 92;
                    UDT::send(cargs.udtsock,(char*) &spkt, sizeof(spkt), 0);
                    logger.log_warning("rejected incoming connection to: %s\n", 
                            utl::dump_inetaddr(&paddr, buf, true));
                    break;
                }
            }

            if (connect(peersock, (sockaddr*) &paddr, sizeof(paddr)) == -1) {
                spkt.cd = 92;
                UDT::send(cargs.udtsock,(char*) &spkt, sizeof(spkt), 0);
                logger.log_warning("canot connect\n");
                break;
            }

            /*
            int rcvbuf = 1024*1024;
            if(setsockopt(peersock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int)))
                logger.log_die("setsockopt: failed\n");
            if(setsockopt(peersock, SOL_SOCKET, SO_SNDBUF, &rcvbuf, sizeof(int)))
                logger.log_die("setsockopt: failed\n");
                */

            logger.log_debug(2, "connected.\n");

            spkt.cd = 90;
            UDT::send(cargs.udtsock,(char*) &spkt, sizeof(spkt), 0);
            logger.log_debug(2, "udt <- soks send\n");
        }
        else { // tcp -> udt

            UDTSOCKET peersock = UDT::socket(pPeeraddr->ai_family, pPeeraddr->ai_socktype, pPeeraddr->ai_protocol);

            cargs.tcpsock = *(int*) servsock;
            cargs.udtsock = peersock;
            
            recv(cargs.tcpsock,&spkt, sizeof(spkt), MSG_WAITALL);

            logger.log_debug(2, "tcp -> socks recv (-)\n");

            if(spkt.vn != 4) {
                logger.log_warning("wrong socks version; connection closed\n");
                logger.log_warning("scoks %d\n", spkt.vn);
                spkt.cd = 91;
                send(cargs.tcpsock,&spkt, sizeof(spkt), 0);
                break;
            }
            if(spkt.cd != 1) {
                logger.log_warning("wrong command - only connect is supported\n");
                spkt.cd = 90;
                send(cargs.tcpsock,&spkt, sizeof(spkt), 0);
                break;
            }
            
            do {
                recv(cargs.tcpsock,&c, 1, 0);

            } while (c != 0);

            logger.log_debug(2, "tcp -> socks recv (+)\n");

            setsockopt(peersock);
            

            sockaddr * bind_addr;
            size_t     addr_len;

            if (globals::is_rendezvous) {
                bind_addr = pPeerRzvLocal->ai_addr;
                addr_len  = pPeerRzvLocal->ai_addrlen;
            }
            else {
                bind_addr = (sockaddr*) &addr_any;
                addr_len  = sizeof(sockaddr_in);
            }
            if (UDT::ERROR == UDT::bind(peersock, bind_addr, addr_len))
            {
                    logger.log_die("udt bind: %s\n", UDT::getlasterror().getErrorMessage());
                    return 0;
            }
            
            logger.log_debug(1, "open UDT connection to: %s\n", utl::dump_inetaddr((sockaddr_in *) pPeeraddr->ai_addr, buf, true));

            if (UDT::ERROR == UDT::connect(peersock, pPeeraddr->ai_addr, pPeeraddr->ai_addrlen))
            {
                logger.log_warning("udt connect: %s\n", UDT::getlasterror().getErrorMessage());
                spkt.cd = 92;
                send(cargs.tcpsock,&spkt, sizeof(spkt), 0);
                break;
            }

            logger.log_debug(1, "udt connected\n");

            UDT::send(peersock,(char*) &spkt, sizeof(spkt), 0);
            UDT::send(peersock,"", sizeof(""), 0);

            logger.log_debug(2, "udt <- soks send\n");

            while(UDT::recv(peersock, (char*) &spkt, sizeof(spkt), 0) == 0);

            logger.log_debug(2, "udt -> soks recv\n");

            send(cargs.tcpsock,&spkt, sizeof(spkt), 0);

            logger.log_debug(2, "tcp <- soks send\n");

            if (spkt.cd != 90) {
                logger.log_err("remote peer socks error\n");
                break;
            }
        }
        
        pthread_create(&rcvthread, NULL, peer2sock_worker, &cargs);
        pthread_create(&sndthread, NULL, sock2peer_worker, &cargs);
        pthread_join(rcvthread, NULL);
        pthread_join(sndthread, NULL);
        //utl::worker(cargs.tcpsock, SOCK_API::embedsockfd(cargs.udtsock, SOCK_API::UDT));
        break;
    }


    //log_debug("closing udt connection ...");
    UDT::close(cargs.udtsock);
    //logger.log_debug("... chosing udt connection : ok");
    //logger.log_debug("closing tcp connection ...");
    close(cargs.tcpsock);
    //logger.log_debug("... closing tcp connection : ok");
    //logger.log_debug("");
    if (globals::debug_level or globals::is_track_connections)
    	logger.log_notice("close %s connection: %s:%s\n", conntype, clienthost, clientservice);
    return NULL;
}
void* sock2peer_worker(void * ar )
{
    cargs_t * pCargs = (cargs_t *) ar;
    char* data;
    data = new char[rcvbuffer];

    struct pollfd pfd[1];

    pfd[0].fd = pCargs->tcpsock;
    pfd[0].events = POLLIN;

    logger.log_debug(2, "start tcp->udt thread\n");

    while (!pCargs->shutdown) {

        int sz, pollr;

        while(EINTR == (pollr = poll(pfd,1,10))) {};
        
        if (pollr == 0)
            continue;
        else if (poll < 0)
            break;

        sz = recv(pCargs->tcpsock, data, rcvbuffer, 0);

        if (sz == -1) {
            logger.log_notice("socks client recv error\n");
            break;
        }
        if (sz == 0) {
            break;
        }
        //logger.log_debug(3, "tcp recv %d bytes\n", sz);
        if (SOCK_API::writen(SOCK_API::embedsockfd(pCargs->udtsock,SOCK_API::UDT), data, sz) < 0) {
            logger.log_err("udt send error: %s\n", UDT::getlasterror().getErrorMessage());
            break;
        }

        char str[globals::dump_message*5+10];
        logger.log_debug(3,"  tcp->udt %d bytes: [%s]\n", sz, 
                utl::dump_str(str, data, rcvbuffer, sz, globals::dump_message));
        //logger.log_debug(3, "udt send %d bytes\n", sz);
    }

    delete [] data;

    pCargs->shutdown = 1;

    //close(cargsp->tcpsock);
    //UDT::close(cargsp->udtsock);

    logger.log_debug(2, "stop tcp->udt thread\n");
    return NULL;
}

void* peer2sock_worker(void* ar)
{
    cargs_t * pCargs = (cargs_t *) ar;
    char* data;
    data = new char[rcvbuffer];

    logger.log_debug(2, "start udt->tcp thread\n");
    
    UDT::setsockopt(pCargs->udtsock, 0, UDT_RCVTIMEO, new int(100), sizeof(int));
    
    while (!pCargs->shutdown)
    {
        int sz;
        do {
            sz =  UDT::recv(pCargs->udtsock, data, rcvbuffer, 0);
        } while (sz == 0 and !pCargs->shutdown);
        //logger.log_debug(3,"udt recv %d bytes\n", sz);
        //if (pCargs->shutdown)
            //break;
        if (UDT::ERROR == sz)
        {
            if (UDT::getlasterror().getErrorCode() == 2001)
                logger.log_notice("udt peer connection closed\n");
            else
                logger.log_err("udt recv: %s", UDT::getlasterror().getErrorMessage());

            pCargs->shutdown = 0;
            break;
        }
        if (SOCK_API::writen(SOCK_API::embedsockfd(pCargs->tcpsock, SOCK_API::TCP), data, sz) == -1 ) {
            cerr << "send error" << endl;
            break;
        }

        char str[globals::dump_message*5+10];
        logger.log_debug(3,"  udt->tcp %d bytes: [%s]\n", sz, 
                utl::dump_str(str, data, rcvbuffer, sz, globals::dump_message));
    }

    delete [] data;
    pCargs->shutdown = 1;

    //close(cargsp->tcpsock);
    //UDT::close(cargsp->udtsock);

    logger.log_debug(2, "stop udt->tcp thread\n");
    return NULL;
}
bool is_custom_value(UDTOpt optcode) {
    UDT_OPTIONS_T::iterator it = udt_options.find(optcode);
    return it == udt_options.end() ? false : true;
}

// Setup UDP buffer sizes.
// Returns "true" if system udp buffer size restrictions was applied.
bool check_udp_buffer(UDTSOCKET sock, UDTOpt optcode) {

    bool sysctl_ok = false;

    int unsigned   size;
    int unsigned * max_size; // maximal system size
    int unsigned * chk_size; // maximal size succesfully binded
    bool         * warned;   // if already warned about size correction.
    int unsigned   so_opt;
    
    int nlen, optsz;
    size_t    *szptr;

    char* die_format = "requested custom %s value is too big. Maximal possible size = %d\n";
    char* warn_format = "default %s = %d is reduced to the maximal possible detected system size = %d\n";
    
    static int unsigned rcv_chk_size = 0;
    static int unsigned snd_chk_size = 0;
    
    static int unsigned rcv_max_size = 0;
    static int unsigned snd_max_size = 0;

    static bool rcv_warned = false;
    static bool snd_warned = false;
    

    assert(optcode == UDP_RCVBUF || optcode == UDP_SNDBUF);
    
    if (optcode == UDP_RCVBUF) {
        max_size = &rcv_max_size;
        chk_size = &rcv_chk_size;
        warned = &rcv_warned;
        so_opt = SO_RCVBUF;
    }
    else {
        max_size = &snd_max_size;
        chk_size = &snd_chk_size;
        warned = &snd_warned;
        so_opt = SO_SNDBUF;
    }
    
    // get requeste size
    if (is_custom_value(optcode))
        size = udt_options[optcode]; 
    else
        UDT::getsockopt(sock, 0, optcode, &size, &optsz);
    
    if (*chk_size > 0 and size <= *chk_size)
        return true; 

    if (0 == *max_size) {
        sockaddr_in sa;
        
        sa.sin_family      = AF_INET;        
        sa.sin_port        = 0;
        sa.sin_addr.s_addr = INADDR_ANY;
        
        int sd = socket(AF_INET, SOCK_DGRAM, 0);
        
        for (int sz = size; sz > 8096; *max_size = sz = sz / 2) {
            if (
                    setsockopt(sd, SOL_SOCKET, so_opt, &sz, sizeof(int)) == 0 and 
                    bind(sd, (sockaddr*) &sa, sizeof(sockaddr_in)) == 0
                    ) {
                close(sd);
                if (*chk_size < sz) *chk_size = sz;
                if (*max_size == 0) return true;
                break;
            }
        }
        close(sd);
    }
    
    // max_size has been initialized above!

    if (size > *max_size) {
        if(is_custom_value(optcode))
            logger.log_die(die_format, optcode == UDP_RCVBUF ? "UDP_RCVBUF" : "UDP_SNDBUF" , *max_size);
        if(!(*warned))
            logger.log_warning(warn_format, optcode == UDP_RCVBUF ? "UDP_RCVBUF" : "UDP_SNDBUF", size, *max_size);
        *warned = true;
        UDT::setsockopt(sock, 0, optcode, max_size, sizeof(int));
        return false;
    }
    else
        UDT::setsockopt(sock, 0, optcode, &size, sizeof(int));

    return true;
}

void setsockopt(UDTSOCKET sock) {

    // set custom UDT options:
    for (UDT_OPTIONS_T::iterator it = udt_options.begin(); it != udt_options.end(); it++)
        UDT::setsockopt(sock, 0,  it->first, &it->second, sizeof(int));

    // work around system buffer size restrictions (typical for FreeBSD)
    check_udp_buffer(sock, UDP_RCVBUF);
    check_udp_buffer(sock, UDP_SNDBUF);

    /*
     UDT::setsockopt(sock, 0, UDT_RCVBUF, new int(rcvbuffer*2), sizeof(int));
     UDT::setsockopt(sock, 0, UDT_SNDBUF, new int(rcvbuffer*2), sizeof(int));
     */

    //// 
    // set RCVTIMEO = 100 ms
    //
    //UDT::setsockopt(sock, 0, UDT_RCVTIMEO, new int(100), sizeof(int));

    ////
    // setup rendezvous mode if -R flag
    //
    if(globals::is_rendezvous)
        UDT::setsockopt(sock, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));

    // set UDP port range to bind if the option UDP_BASEPORT exists (???)
#ifdef UDP_BASEPORT_OPTION
    if (baseport) UDT::setsockopt(*sock, 0, UDP_BASEPORT, new int(baseport), sizeof(int));
    if (naxport)  UDT::setsockopt(*sock, 0, UDP_POOLSIZE, new int(maxport-baseport), sizeof(int));
#endif

    // {"Vegas","TCP","ScalableTCP","HSTCP","BiCTCP", "Westwood", "FAST"};

#ifdef UDT_CCC_OPTION
    if (! strcasecmp(ccc, "")) {
        //UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CUDTCC>, sizeof(CCCFactory<CUDTCC>));
    }
    else if (! strcasecmp(ccc, "UDT")) {
        UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CUDTCC>, sizeof(CCCFactory<CUDTCC>));
    }
    else if (! strcasecmp(ccc, "Vegas")) {
        UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CVegas>, sizeof(CCCFactory<CVegas>));
    }
    else if (! strcasecmp(ccc, "TCP")) {
        UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CTCP>, sizeof(CCCFactory<CTCP>));
    }
    else if (! strcasecmp(ccc, "ScalableTCP")) {
        UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CScalableTCP>, sizeof(CCCFactory<CScalableTCP>));
    }
    else if (! strcasecmp(ccc, "HSTCP")) {
        UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CHSTCP>, sizeof(CCCFactory<CHSTCP>));
    }
    else if (! strcasecmp(ccc, "BiCTCP")) {
        UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CBiCTCP>, sizeof(CCCFactory<CBiCTCP>));
    }
    else if (! strcasecmp(ccc, "Westwood")) {
        UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CWestwood>, sizeof(CCCFactory<CWestwood>));
    }
    else if (! strcasecmp(ccc, "FAST")) {
        UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<CFAST>, sizeof(CCCFactory<CFAST>));
    }
    else {
        cerr << "\nunsupprted congetion control class (option -c): " << ccc<< endl;
        exit(-1);
    }
#endif
}

int parse_udt_option (char * optstr) {
    char * optname = (char*) malloc(strlen(optstr));
    int    optval;

    sscanf(optstr, "%[a-zA-Z_]=%d", optname, &optval);
    {
        char q = optstr[strlen(optstr)-1];
        if      (q=='k' or q=='K')
            optval *= 1024;
        else if (q=='m'or q=='M')
            optval *= 1024*1024;
    }
    for(int i = 0; i < sizeof(UDT_OPTIONS_LIST)/sizeof(*UDT_OPTIONS_LIST); i++) {
        if(!strcasecmp(UDT_OPTIONS_LIST[i].optname, optname)) {
            udt_options[UDT_OPTIONS_LIST[i].optcode] = optval;
            return 0;
        };
    }
    logger.log_die("\nUnknown UDT option: %s\n", optname);
    exit(1);
}

void exit_handler(int sig) {
    logger.log_debug("got signal %d\n", sig);
	kill(0,SIGTERM);
	while(wait(NULL) != -1);
    logger.log_die("");
}
