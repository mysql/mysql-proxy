/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */
 

#ifndef _NETWORK_ADDRESS_H_
#define _NETWORK_ADDRESS_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#ifndef _WIN32
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>     /** struct sockaddr_in */
#endif
#include <netinet/tcp.h>

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>         /** struct sockaddr_un */
#endif
#include <sys/socket.h>     /** struct sockaddr (freebsd and hp/ux need it) */
#else
#include <winsock2.h>

#define socklen_t int
#endif

#include "network-exports.h"

typedef struct {
	union {
		struct sockaddr_in ipv4;
		struct sockaddr_in6 ipv6;
#ifdef HAVE_SYS_UN_H
		struct sockaddr_un un;
#endif
		struct sockaddr common;
	} addr;

	GString *name; 

	socklen_t len;
} network_address;

NETWORK_API network_address *network_address_new(void);
NETWORK_API void network_address_free(network_address *);
NETWORK_API void network_address_reset(network_address *addr);
NETWORK_API network_address *network_address_copy(network_address *dst, network_address *src);
NETWORK_API gint network_address_set_address(network_address *addr, gchar *address);
NETWORK_API gint network_address_refresh_name(network_address *addr);
NETWORK_API gint network_address_is_local(network_address *dst_addr, network_address *src_addr);

#endif
