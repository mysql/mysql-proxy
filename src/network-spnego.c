#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include "glib-ext.h"

#include "network-packet.h" /* for mysql-packet */
#include "network-asn1.h"
#include "network-spnego.h"

#define C(x) (x), sizeof(x) - 1
#define S(x) (x)->str, (x)->len

gboolean
network_spnego_proto_get_negtokeninit(network_packet *packet, gpointer _udata, GError **gerr) {
	ASN1Identifier seq_id;
	ASN1Length seq_len;
	gsize end_offset;

	if (FALSE == network_asn1_proto_get_header(packet, &seq_id, &seq_len, gerr)) {
		return FALSE;
	}

	/* next should be a SEQUENCE */
	if (seq_id.klass != ASN1_IDENTIFIER_KLASS_UNIVERSAL ||
	    seq_id.value != ASN1_IDENTIFIER_UNIVERSAL_SEQUENCE) {
		g_set_error(gerr,
				NETWORK_ASN1_ERROR,
				NETWORK_ASN1_ERROR_INVALID,
				"expected a sequence");
		return FALSE;
	}

	end_offset = packet->offset + seq_len;

	while (packet->offset < end_offset) {
		ASN1Identifier app_id;
		ASN1Length app_len;
		ASN1Identifier mech_seq_id;
		ASN1Length mech_seq_len;
		ASN1Length mech_seq_end_offset;
		ASN1Identifier mech_token_id;
		ASN1Length mech_token_len;

		if (FALSE == network_asn1_proto_get_header(packet, &app_id, &app_len, gerr)) {
			return FALSE;
		}

		if (app_id.klass != ASN1_IDENTIFIER_KLASS_CONTEXT_SPECIFIC) {
			g_set_error(gerr,
					NETWORK_ASN1_ERROR,
					NETWORK_ASN1_ERROR_INVALID,
					"expected a context specific tag");

			return FALSE;
		}

		switch (app_id.value) {
		case 0: /* MechTypes */
			if (FALSE == network_asn1_proto_get_header(packet, &mech_seq_id, &mech_seq_len, gerr)) {
				return FALSE;
			}

			if (mech_seq_id.klass != ASN1_IDENTIFIER_KLASS_UNIVERSAL ||
			    mech_seq_id.value != ASN1_IDENTIFIER_UNIVERSAL_SEQUENCE) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}

			mech_seq_end_offset = packet->offset + mech_seq_len;

			while (packet->offset < mech_seq_end_offset) {
				ASN1Identifier mech_seq_oid_id;
				ASN1Length mech_seq_oid_len;
				GString *oid;

				if (FALSE == network_asn1_proto_get_header(packet, &mech_seq_oid_id, &mech_seq_oid_len, gerr)) {
					return FALSE;
				}

				if (mech_seq_oid_id.klass != ASN1_IDENTIFIER_KLASS_UNIVERSAL ||
				    mech_seq_oid_id.value != ASN1_IDENTIFIER_UNIVERSAL_OID) {
					g_set_error(gerr,
							NETWORK_ASN1_ERROR,
							NETWORK_ASN1_ERROR_INVALID,
							"%s: ...", 
							G_STRLOC);

					return FALSE;
				}

				oid = g_string_new(NULL);
				if (FALSE == network_asn1_proto_get_oid(packet, mech_seq_oid_len, oid, gerr)) {
					g_string_free(oid, TRUE);
					return FALSE;
				}
				g_string_free(oid, TRUE);
			}

			break;
		case 2: /* mechToken */
			if (FALSE == network_asn1_proto_get_header(packet, &mech_token_id, &mech_token_len, gerr)) {
				return FALSE;
			}

			if (mech_token_id.klass != ASN1_IDENTIFIER_KLASS_UNIVERSAL ||
			    mech_token_id.value != ASN1_IDENTIFIER_UNIVERSAL_OCTET_STREAM) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}

			if (FALSE == network_packet_skip(packet, mech_token_len)) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}

			break;
		default:
			g_set_error(gerr,
					NETWORK_ASN1_ERROR,
					NETWORK_ASN1_ERROR_UNSUPPORTED,
					"right now only MechTypes and mechToken are supported");

			return FALSE;
		}
	}

	return TRUE;
}

gboolean
network_spnego_proto_get_negtokenresponse(network_packet *packet, gpointer _udata, GError **gerr) {
	ASN1Identifier seq_id;
	ASN1Length seq_len;
	gsize end_offset;

	if (FALSE == network_asn1_proto_get_header(packet, &seq_id, &seq_len, gerr)) {
		return FALSE;
	}

	/* next should be a SEQUENCE */
	if (seq_id.klass != ASN1_IDENTIFIER_KLASS_UNIVERSAL ||
	    seq_id.value != ASN1_IDENTIFIER_UNIVERSAL_SEQUENCE) {
		g_set_error(gerr,
				NETWORK_ASN1_ERROR,
				NETWORK_ASN1_ERROR_INVALID,
				"expected a sequence");
		return FALSE;
	}

	end_offset = packet->offset + seq_len;

	while (packet->offset < end_offset) {
		ASN1Identifier app_id;
		ASN1Length app_len;
		ASN1Identifier sub_id;
		ASN1Length sub_len;
		guint8 negState;

		if (FALSE == network_asn1_proto_get_header(packet, &app_id, &app_len, gerr)) {
			return FALSE;
		}

		if (app_id.klass != ASN1_IDENTIFIER_KLASS_CONTEXT_SPECIFIC) {
			g_set_error(gerr,
					NETWORK_ASN1_ERROR,
					NETWORK_ASN1_ERROR_INVALID,
					"expected a context specific tag");

			return FALSE;
		}

		switch (app_id.value) {
		case 0: /* negState */
			if (FALSE == network_asn1_proto_get_header(packet, &sub_id, &sub_len, gerr)) {
				return FALSE;
			}

			if (sub_id.klass != ASN1_IDENTIFIER_KLASS_UNIVERSAL ||
			    sub_id.value != ASN1_IDENTIFIER_UNIVERSAL_ENUM) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}

			/* we should only get one byte */

			if (FALSE == network_packet_get_data(packet, &negState, 1)) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}
			g_debug("%s: negState = %d",
					G_STRLOC, negState);

			break;
		case 1: /* supportedMech */
			if (FALSE == network_asn1_proto_get_header(packet, &sub_id, &sub_len, gerr)) {
				return FALSE;
			}

			if (sub_id.klass != ASN1_IDENTIFIER_KLASS_UNIVERSAL ||
			    sub_id.value != ASN1_IDENTIFIER_UNIVERSAL_OID) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}

			if (FALSE == network_packet_skip(packet, sub_len)) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}

			break;
		case 2: /* responseToken */
			if (FALSE == network_asn1_proto_get_header(packet, &sub_id, &sub_len, gerr)) {
				return FALSE;
			}

			if (sub_id.klass != ASN1_IDENTIFIER_KLASS_UNIVERSAL ||
			    sub_id.value != ASN1_IDENTIFIER_UNIVERSAL_OCTET_STREAM) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}

			if (FALSE == network_packet_skip(packet, sub_len)) {
				g_set_error(gerr,
						NETWORK_ASN1_ERROR,
						NETWORK_ASN1_ERROR_INVALID,
						"%s: ...", 
						G_STRLOC);

				return FALSE;
			}


			break;
		default:
			g_set_error(gerr,
					NETWORK_ASN1_ERROR,
					NETWORK_ASN1_ERROR_UNSUPPORTED,
					"right now only MechTypes and mechToken are supported");

			return FALSE;
		}
	}

	return TRUE;
}

/**
 * get the inner token of a SPNEGO message
 */
gboolean
network_spnego_proto_get_init_token_inner(network_packet *packet, gpointer _udata, GError **gerr) {
	ASN1Identifier spnego_id;
	ASN1Length spnego_len;

	if (FALSE == network_asn1_proto_get_header(packet, &spnego_id, &spnego_len, gerr)) {
		return FALSE;
	}

	if (spnego_id.klass == ASN1_IDENTIFIER_KLASS_CONTEXT_SPECIFIC &&
	    spnego_id.value == 0) {
		/* NegTokenInit */
		return network_spnego_proto_get_negtokeninit(packet, _udata, gerr);
	} else if (spnego_id.klass == ASN1_IDENTIFIER_KLASS_CONTEXT_SPECIFIC &&
	    spnego_id.value == 1) {
		/* NegTokenResponse */
		return network_spnego_proto_get_negtokenresponse(packet, _udata, gerr);
	} else {
		return FALSE;
	}
}

gboolean
network_gssapi_proto_get_message_header(network_packet *packet, GString *oid, GError **gerr) {
	ASN1Identifier gss_id;
	ASN1Length gss_len;
	ASN1Identifier oid_id;
	ASN1Length oid_len;

	if (FALSE == network_asn1_proto_get_header(packet, &gss_id, &gss_len, gerr)) {
		return FALSE;
	}

	/* first we have a GSS-API header */
	g_assert_cmpint(gss_id.klass, ==, ASN1_IDENTIFIER_KLASS_APPLICATION);
	g_assert_cmpint(gss_id.value, ==, 0);

	/* we should have as much data left in the packet as announced */
	if (!network_packet_has_more_data(packet, gss_len)) {
		g_set_error(gerr, 
			NETWORK_ASN1_ERROR,
			NETWORK_ASN1_ERROR_INVALID,
			"length field invalid");
		return FALSE;
	}

	/* good, now we should have a OID next */
	if (FALSE == network_asn1_proto_get_header(packet, &oid_id, &oid_len, gerr)) {
		return FALSE;
	}

	/* first we have a GSS-API header */
	g_assert_cmpint(oid_id.klass, ==, ASN1_IDENTIFIER_KLASS_UNIVERSAL);
	g_assert_cmpint(oid_id.value, ==, ASN1_IDENTIFIER_UNIVERSAL_OID); /* OID */

	if (FALSE == network_asn1_proto_get_oid(packet, oid_len, oid, gerr)) {
		return FALSE;
	}

	return TRUE;
}

