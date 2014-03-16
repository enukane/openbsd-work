/*
 * sys-bsd.c - System-dependent procedures for setting up
 * PPP interfaces on bsd-4.4-ish systems (including 386BSD, NetBSD, etc.)
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $OpenBSD: arp.c,v 1.16 2009/07/02 16:08:29 claudio Exp $
 *
 */

/*
 * TODO:
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <errno.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "defs.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "lqr.h"
#include "hdlc.h"
#include "ncpaddr.h"
#include "ipcp.h"
#include "ipv6cp.h"
#include "descriptor.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#include "ncp.h"
#include "filter.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "iface.h"
#include "arp.h"

/*
 * SET_SA_FAMILY - set the sa_family field of a struct sockaddr,
 * if it exists.
 */
#define SET_SA_FAMILY(addr, family)		\
    memset((char *) &(addr), '\0', sizeof(addr));	\
    addr.sa_family = (family); 			\
    addr.sa_len = sizeof(addr);


#if RTM_VERSION >= 3

/*
 * arp_SetProxy - Make a proxy ARP entry for the peer.
 */
static struct {
  struct rt_msghdr hdr;
  struct sockaddr_inarp dst;
  struct sockaddr_dl hwa;
  char extra[128];
} arpmsg;

static int
arp_ProxySub(struct bundle *bundle, struct in_addr addr, int add, int s)
{
  int routes;

  /*
   * Get the hardware address of an interface on the same subnet as our local
   * address.
   */

  memset(&arpmsg, 0, sizeof arpmsg);
  if (!arp_EtherAddr(s, addr, &arpmsg.hwa, 0)) {
    log_Printf(LogWARN, "%s: Cannot determine ethernet address for proxy ARP\n",
	       inet_ntoa(addr));
    return 0;
  }
  routes = ID0socket(PF_ROUTE, SOCK_RAW, AF_INET);
  if (routes < 0) {
    log_Printf(LogERROR, "arp_SetProxy: opening routing socket: %s\n",
	      strerror(errno));
    return 0;
  }
  arpmsg.hdr.rtm_type = add ? RTM_ADD : RTM_DELETE;
  arpmsg.hdr.rtm_flags = RTF_ANNOUNCE | RTF_HOST | RTF_STATIC;
  arpmsg.hdr.rtm_version = RTM_VERSION;
  arpmsg.hdr.rtm_seq = ++bundle->routing_seq;
  arpmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
  arpmsg.hdr.rtm_inits = RTV_EXPIRE;
  arpmsg.dst.sin_len = sizeof(struct sockaddr_inarp);
  arpmsg.dst.sin_family = AF_INET;
  arpmsg.dst.sin_addr.s_addr = addr.s_addr;
  arpmsg.dst.sin_other = SIN_PROXY;

  arpmsg.hdr.rtm_msglen = (char *) &arpmsg.hwa - (char *) &arpmsg
    + arpmsg.hwa.sdl_len;


  if (ID0write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0 &&
      !(!add && errno == ESRCH)) {
    log_Printf(LogERROR, "%s proxy arp entry %s: %s\n",
	add ? "Add" : "Delete", inet_ntoa(addr), strerror(errno));
    close(routes);
    return 0;
  }
  close(routes);
  return 1;
}

int
arp_SetProxy(struct bundle *bundle, struct in_addr addr, int s)
{
  return (arp_ProxySub(bundle, addr, 1, s));
}

/*
 * arp_ClearProxy - Delete the proxy ARP entry for the peer.
 */
int
arp_ClearProxy(struct bundle *bundle, struct in_addr addr, int s)
{
  return (arp_ProxySub(bundle, addr, 0, s));
}

#else				/* RTM_VERSION */

/*
 * arp_SetProxy - Make a proxy ARP entry for the peer.
 */
int
arp_SetProxy(struct bundle *bundle, struct in_addr addr, int s)
{
  struct arpreq arpreq;
  struct {
    struct sockaddr_dl sdl;
    char space[128];
  }      dls;

  memset(&arpreq, '\0', sizeof arpreq);

  /*
   * Get the hardware address of an interface on the same subnet as our local
   * address.
   */
  if (!arp_EtherAddr(s, addr, &dls.sdl, 1)) {
    log_Printf(LOG_PHASE_BIT, "Cannot determine ethernet address for "
               "proxy ARP\n");
    return 0;
  }
  arpreq.arp_ha.sa_len = sizeof(struct sockaddr);
  arpreq.arp_ha.sa_family = AF_UNSPEC;
  memcpy(arpreq.arp_ha.sa_data, LLADDR(&dls.sdl), dls.sdl.sdl_alen);
  SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
  ((struct sockaddr_in *)&arpreq.arp_pa)->sin_addr.s_addr = addr.s_addr;
  arpreq.arp_flags = ATF_PERM | ATF_PUBL;
  if (ID0ioctl(s, SIOCSARP, (caddr_t) & arpreq) < 0) {
    log_Printf(LogERROR, "arp_SetProxy: ioctl(SIOCSARP): %s\n",
               strerror(errno));
    return 0;
  }
  return 1;
}

/*
 * arp_ClearProxy - Delete the proxy ARP entry for the peer.
 */
int
arp_ClearProxy(struct bundle *bundle, struct in_addr addr, int s)
{
  struct arpreq arpreq;

  memset(&arpreq, '\0', sizeof arpreq);
  SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
  ((struct sockaddr_in *)&arpreq.arp_pa)->sin_addr.s_addr = addr.s_addr;
  if (ID0ioctl(s, SIOCDARP, (caddr_t) & arpreq) < 0) {
    log_Printf(LogERROR, "arp_ClearProxy: ioctl(SIOCDARP): %s\n",
               strerror(errno));
    return 0;
  }
  return 1;
}

#endif				/* RTM_VERSION */


/*
 * arp_EtherAddr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */

int
arp_EtherAddr(int s, struct in_addr ipaddr, struct sockaddr_dl *hwaddr,
              int verbose)
{
  struct sockaddr_dl *dl = NULL;
  struct ifaddrs *ifa, *ifap;
  int skip = 1;

  if (getifaddrs(&ifap) != 0) {
    log_Printf(LogERROR, "arp_EtherAddr: getifaddrs: %s\n", strerror(errno));
    return 0;
  }

  for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr->sa_family == AF_LINK) {
      dl = (struct sockaddr_dl *)ifa->ifa_addr;
      skip = (ifa->ifa_flags & (IFF_UP | IFF_BROADCAST | IFF_POINTOPOINT |
            IFF_NOARP | IFF_LOOPBACK)) != (IFF_UP | IFF_BROADCAST);
      continue;
    }
    if (skip)
      /* Skip unusable interface */
      continue;

    /* Found a candidate.  Do the addresses match ? */
    if (log_IsKept(LogDEBUG))
      log_Printf(LogDEBUG, "%.*s interface is a candidate for proxy\n",
                dl->sdl_nlen, dl->sdl_data);

    if (ifa->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in *addr, *netmask;

      addr = (struct sockaddr_in *)ifa->ifa_addr;
      netmask = (struct sockaddr_in *)ifa->ifa_netmask;

      if (log_IsKept(LogDEBUG)) {
        char a[16];

        strncpy(a, inet_ntoa(netmask->sin_addr), sizeof a - 1);
        a[sizeof a - 1] = '\0';
        log_Printf(LogDEBUG, "Check addr %s, mask %s\n",
                   inet_ntoa(addr->sin_addr), a);
      }

      if ((addr->sin_addr.s_addr & netmask->sin_addr.s_addr) ==
          (ipaddr.s_addr & netmask->sin_addr.s_addr)) {
        log_Printf(verbose ? LogPHASE : LogDEBUG,
                   "Found interface %.*s for %s\n", dl->sdl_nlen,
                   dl->sdl_data, inet_ntoa(ipaddr));
        memcpy(hwaddr, dl, dl->sdl_len);
        freeifaddrs(ifap);
        return 1;
      }
    }
  }
  freeifaddrs(ifap);

  return 0;
}