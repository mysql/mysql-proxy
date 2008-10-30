/* $%BEGINLICENSE%$
 Copyright (C) 2008 MySQL AB, 2008 Sun Microsystems, Inc

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
#ifndef _NETWORK_MYSQLD_MASTERINFO_H_
#define _NETWORK_MYSQLD_MASTERINFO_H_

#include <glib.h>

#include "network-exports.h"
#include "network-mysqld-proto.h"

typedef struct {
	GString  *master_log_file;
	guint32   master_log_pos;
	GString  *master_host;
	GString  *master_user;
	GString  *master_password;
	guint32   master_port;
	guint32   master_connect_retry;

	guint32   master_ssl;          /* if ssl is compiled in */
	GString  *master_ssl_ca;
	GString  *master_ssl_capath;
	GString  *master_ssl_cert;
	GString  *master_ssl_cipher;
	GString  *master_ssl_key;

	guint32   master_ssl_verify_server_cert; /* 5.1.16+ */
} network_mysqld_masterinfo_t;

NETWORK_API network_mysqld_masterinfo_t * network_mysqld_masterinfo_new(void);
NETWORK_API int network_mysqld_masterinfo_get(network_packet *packet, network_mysqld_masterinfo_t *info);
NETWORK_API int network_mysqld_masterinfo_append(GString *packet, network_mysqld_masterinfo_t *info);
NETWORK_API void network_mysqld_masterinfo_free(network_mysqld_masterinfo_t *info);

#endif
