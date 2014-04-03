/*
 * getifaddrs.h
 *
 *  Created on: May 18, 2010
 *      Author: wk
 */

#ifndef _GETIFADDRS_H_
#define _GETIFADDRS_H_

struct ifaddrs {
        struct ifaddrs   *ifa_next;         /* Pointer to next struct */
        char             *ifa_name;         /* Interface name */
        unsigned int     ifa_flags;         /* Interface flags */
        struct sockaddr  *ifa_addr;         /* Interface address */
        struct sockaddr  *ifa_netmask;      /* Interface netmask */
#undef ifa_dstaddr
        struct sockaddr  *ifa_dstaddr;      /* P2P interface destination */
        void             *ifa_data;         /* Address specific data */
};

void freeifaddrs(struct ifaddrs *ifp);
int getifaddrs(struct ifaddrs **ifap);

#endif /* _GETIFADDRS_H_ */
