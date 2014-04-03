/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Jeremy Allison 2007
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define SOCKET_WRAPPER_NOT_REPLACE

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <net/if.h>
#include <sys/time.h>


#ifndef HAVE_GETIFADDRS
#include <sys/sockio.h>
#include <getifaddrs.h>

void freeifaddrs(struct ifaddrs *ifp)
{
	if (ifp != NULL) {
		free(ifp->ifa_name);
		free(ifp->ifa_addr);
		free(ifp->ifa_netmask);
		free(ifp->ifa_dstaddr);
		freeifaddrs(ifp->ifa_next);
		free(ifp);
	}
}

static struct sockaddr *sockaddr_dup(struct sockaddr *sa)
{
	struct sockaddr *ret;
	socklen_t socklen;
#ifdef HAVE_SOCKADDR_SA_LEN
	socklen = sa->sa_len;
#else
	socklen = sizeof(struct sockaddr_storage);
#endif
	ret = (struct sockaddr *) calloc(1, socklen);
	if (ret == NULL)
		return NULL;
	memcpy(ret, sa, socklen);
	return ret;
}


/* this works for Linux 2.2, Solaris 2.5, SunOS4, HPUX 10.20, OSF1
   V4.0, Ultrix 4.4, SCO Unix 3.2, IRIX 6.4 and FreeBSD 3.2.

   It probably also works on any BSD style system.  */

int getifaddrs(struct ifaddrs **ifap)
{
	struct ifconf ifc;
	char buff[8192];
	int fd, i, n;
	struct ifreq *ifr=NULL;
	struct ifaddrs *curif;
	struct ifaddrs *lastif = NULL;

	*ifap = NULL;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return -1;
	}
  
	ifc.ifc_len = sizeof(buff);
	ifc.ifc_buf = buff;

	if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
		close(fd);
		return -1;
	} 

	ifr = ifc.ifc_req;
  
	n = ifc.ifc_len / sizeof(struct ifreq);

	/* Loop through interfaces, looking for given IP address */
	for (i=n-1; i>=0; i--) {
		if (ioctl(fd, SIOCGIFFLAGS, &ifr[i]) == -1) {
			freeifaddrs(*ifap);
			return -1;
		}

		curif = (struct ifaddrs*) calloc(1, sizeof(struct ifaddrs));
		curif->ifa_name = strdup(ifr[i].ifr_name);
		curif->ifa_flags = ifr[i].ifr_flags;
		curif->ifa_dstaddr = NULL;
		curif->ifa_data = NULL;
		curif->ifa_next = NULL;

		curif->ifa_addr = NULL;
		if (ioctl(fd, SIOCGIFADDR, &ifr[i]) != -1) {
			curif->ifa_addr = sockaddr_dup(&ifr[i].ifr_addr);
		}

		curif->ifa_netmask = NULL;
		if (ioctl(fd, SIOCGIFNETMASK, &ifr[i]) != -1) {
			curif->ifa_netmask = sockaddr_dup(&ifr[i].ifr_addr);
		}

		if (lastif == NULL) {
			*ifap = curif;
		} else {
			lastif->ifa_next = curif;
		}
		lastif = curif;
	}

	close(fd);

	return 0;
}  

#endif /* HAVE_GETIFADDR */
