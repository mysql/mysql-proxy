#ifndef __NETWORK_SPNEGO_H__
#define __NETWORK_SPNEGO_H__

#include <glib.h>

gboolean
network_gssapi_proto_get_message_header(network_packet *packet, GString *oid, GError **gerr);

gboolean
network_spnego_proto_get_init_token_inner(network_packet *packet, gpointer _udata, GError **gerr);

gboolean
network_spnego_proto_get_negtokenresponse(network_packet *packet, gpointer _udata, GError **gerr);

gboolean
network_spnego_proto_get_negtokeninit(network_packet *packet, gpointer _udata, GError **gerr);

#endif
