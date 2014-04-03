#include <config.h>
#include <udtgate.h>

int         globals::net_access = 0;
int         globals::debug_level = 0;
int         globals::dump_message = 0;
bool        globals::is_track_connections = false;
bool        globals::is_rendezvous = false;
bool        globals::is_demonize = false;
bool        globals::is_custom_acl = false;

acl_table globals::custom_acl;

#ifdef UDP_BASEPORT_OPTION
int     globals::baseport = 0;
int     globals::maxport = 0;
#endif
char*   globals::app_ident   = "?app_ident?";
char*   globals::sock_ident =  "?sock_ident?";
char*   globals::peer_ident  = "?peer_ident?";
char*   globals::serv_ident  = app_ident;
