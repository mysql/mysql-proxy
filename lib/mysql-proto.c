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
 

/**
 * expose the chassis functions into the lua space
 */


#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "network-mysqld-proto.h"
#include "network-mysqld-packet.h"
#include "network-mysqld-masterinfo.h"
#include "glib-ext.h"
#include "lua-env.h"

#define C(x) x, sizeof(x) - 1
#define S(x) x->str, x->len

#define LUA_IMPORT_INT(x, y) \
	lua_getfield_literal(L, -1, C(G_STRINGIFY(y))); \
	if (!lua_isnil(L, -1)) { \
		x->y = lua_tointeger(L, -1); \
	} \
	lua_pop(L, 1);

#define LUA_IMPORT_STR(x, y) \
	lua_getfield_literal(L, -1, C(G_STRINGIFY(y))); \
	if (!lua_isnil(L, -1)) { \
		size_t s_len; \
		const char *s = lua_tolstring(L, -1, &s_len); \
		g_string_assign_len(x->y, s, s_len); \
	} \
	lua_pop(L, 1);

#define LUA_EXPORT_INT(x, y) \
	lua_pushinteger(L, x->y); \
	lua_setfield(L, -2, G_STRINGIFY(y)); 

#define LUA_EXPORT_STR(x, y) \
	if (x->y->len) { \
		lua_pushlstring(L, S(x->y)); \
		lua_setfield(L, -2, G_STRINGIFY(y)); \
	}

static int lua_proto_get_err_packet (lua_State *L) {
	size_t packet_len;
	const char *packet_str = luaL_checklstring(L, 1, &packet_len);
	network_mysqld_err_packet_t *err_packet;
	network_packet packet;
	GString s;
	int err = 0;

	s.str = (char *)packet_str;
	s.len = packet_len;

	packet.data = &s;
	packet.offset = 0;

	err_packet = network_mysqld_err_packet_new();

	err = err || network_mysqld_proto_get_err_packet(&packet, err_packet);
	if (err) {
		network_mysqld_err_packet_free(err_packet);

		luaL_error(L, "%s: network_mysqld_proto_get_err_packet() failed", G_STRLOC);
		return 0;
	}

	lua_newtable(L);

	LUA_EXPORT_STR(err_packet, errmsg);
	LUA_EXPORT_STR(err_packet, sqlstate);
	LUA_EXPORT_INT(err_packet, errcode);

	network_mysqld_err_packet_free(err_packet);

	return 1;
}

static int lua_proto_append_err_packet (lua_State *L) {
	GString *packet;
	network_mysqld_err_packet_t *err_packet;

	luaL_checktype(L, 1, LUA_TTABLE);

	err_packet = network_mysqld_err_packet_new();

	LUA_IMPORT_STR(err_packet, errmsg);
	LUA_IMPORT_STR(err_packet, sqlstate);
	LUA_IMPORT_INT(err_packet, errcode);

	packet = g_string_new(NULL);	
	network_mysqld_proto_append_err_packet(packet, err_packet);

	lua_pushlstring(L, S(packet));
	
	network_mysqld_err_packet_free(err_packet);

	g_string_free(packet, TRUE);

	return 1;
}

static int lua_proto_get_ok_packet (lua_State *L) {
	size_t packet_len;
	const char *packet_str = luaL_checklstring(L, 1, &packet_len);
	network_mysqld_ok_packet_t *ok_packet;
	network_packet packet;
	GString s;
	int err = 0;

	s.str = (char *)packet_str;
	s.len = packet_len;

	packet.data = &s;
	packet.offset = 0;

	ok_packet = network_mysqld_ok_packet_new();

	err = err || network_mysqld_proto_get_ok_packet(&packet, ok_packet);
	if (err) {
		network_mysqld_ok_packet_free(ok_packet);

		luaL_error(L, "%s: network_mysqld_proto_get_ok_packet() failed", G_STRLOC);
		return 0;
	}

	lua_newtable(L);
	LUA_EXPORT_INT(ok_packet, server_status);
	LUA_EXPORT_INT(ok_packet, insert_id);
	LUA_EXPORT_INT(ok_packet, warnings);
	LUA_EXPORT_INT(ok_packet, affected_rows);

	network_mysqld_ok_packet_free(ok_packet);

	return 1;
}

static int lua_proto_get_masterinfo (lua_State *L) {
	size_t packet_len;
	const char *packet_str = luaL_checklstring(L, 1, &packet_len);
	network_mysqld_masterinfo_t *info;

	network_packet packet;
	GString s;
	int err = 0;

	s.str = (char *)packet_str;
	s.len = packet_len;

	packet.data = &s;
	packet.offset = 0;

	info = network_mysqld_masterinfo_new();

	err = err || network_mysqld_masterinfo_get(&packet, info);
	
	if (err) {
		network_mysqld_masterinfo_free(info);
		luaL_error(L, "%s: network_mysqld_masterinfo_get() failed", G_STRLOC);
		return 0;
	}

	lua_newtable(L);

	LUA_EXPORT_STR(info, master_log_file);
	LUA_EXPORT_INT(info, master_log_pos);
	LUA_EXPORT_STR(info, master_host);
	LUA_EXPORT_STR(info, master_user);
	LUA_EXPORT_STR(info, master_password);
	LUA_EXPORT_INT(info, master_port);
	LUA_EXPORT_INT(info, master_connect_retry);
	LUA_EXPORT_INT(info, master_ssl);
	LUA_EXPORT_INT(info, master_ssl_verify_server_cert);

	network_mysqld_masterinfo_free(info);

	return 1;
}

static int lua_proto_append_ok_packet (lua_State *L) {
	GString *packet;
	network_mysqld_ok_packet_t *ok_packet;

	luaL_checktype(L, 1, LUA_TTABLE);

	ok_packet = network_mysqld_ok_packet_new();

	LUA_IMPORT_INT(ok_packet, server_status);
	LUA_IMPORT_INT(ok_packet, insert_id);
	LUA_IMPORT_INT(ok_packet, warnings);
	LUA_IMPORT_INT(ok_packet, affected_rows);

	packet = g_string_new(NULL);	
	network_mysqld_proto_append_ok_packet(packet, ok_packet);

	lua_pushlstring(L, S(packet));
	
	network_mysqld_ok_packet_free(ok_packet);
	
	g_string_free(packet, TRUE);

	return 1;
}

static int lua_proto_get_eof_packet (lua_State *L) {
	size_t packet_len;
	const char *packet_str = luaL_checklstring(L, 1, &packet_len);
	network_mysqld_eof_packet_t *eof_packet;
	network_packet packet;
	GString s;
	int err = 0;

	s.str = (char *)packet_str;
	s.len = packet_len;

	packet.data = &s;
	packet.offset = 0;

	eof_packet = network_mysqld_eof_packet_new();

	err = err || network_mysqld_proto_get_eof_packet(&packet, eof_packet);
	if (err) {
		network_mysqld_eof_packet_free(eof_packet);

		luaL_error(L, "%s: network_mysqld_proto_get_eof_packet() failed", G_STRLOC);
		return 0;
	}

	lua_newtable(L);
	LUA_EXPORT_INT(eof_packet, server_status);
	LUA_EXPORT_INT(eof_packet, warnings);

	network_mysqld_eof_packet_free(eof_packet);

	return 1;
}

static int lua_proto_append_eof_packet (lua_State *L) {
	GString *packet;
	network_mysqld_eof_packet_t *eof_packet;

	luaL_checktype(L, 1, LUA_TTABLE);

	eof_packet = network_mysqld_eof_packet_new();

	LUA_IMPORT_INT(eof_packet, server_status);
	LUA_IMPORT_INT(eof_packet, warnings);

	packet = g_string_new(NULL);	
	network_mysqld_proto_append_eof_packet(packet, eof_packet);

	lua_pushlstring(L, S(packet));
	
	network_mysqld_eof_packet_free(eof_packet);
	
	g_string_free(packet, TRUE);

	return 1;
}

static int lua_proto_get_response_packet (lua_State *L) {
	size_t packet_len;
	const char *packet_str = luaL_checklstring(L, 1, &packet_len);
	network_mysqld_auth_response *auth_response;
	network_packet packet;
	GString s;
	int err = 0;

	s.str = (char *)packet_str;
	s.len = packet_len;

	packet.data = &s;
	packet.offset = 0;

	auth_response = network_mysqld_auth_response_new();

	err = err || network_mysqld_proto_get_auth_response(&packet, auth_response);
	if (err) {
		network_mysqld_auth_response_free(auth_response);

		luaL_error(L, "%s: network_mysqld_proto_get_auth_response() failed", G_STRLOC);
		return 0;
	}

	lua_newtable(L);
	LUA_EXPORT_INT(auth_response, capabilities);
	LUA_EXPORT_INT(auth_response, max_packet_size);
	LUA_EXPORT_INT(auth_response, charset);

	LUA_EXPORT_STR(auth_response, username);
	LUA_EXPORT_STR(auth_response, response);
	LUA_EXPORT_STR(auth_response, database);

	network_mysqld_auth_response_free(auth_response);

	return 1;
}

static int lua_proto_append_response_packet (lua_State *L) {
	GString *packet;
	network_mysqld_auth_response *auth_response;

	luaL_checktype(L, 1, LUA_TTABLE);

	packet = g_string_new(NULL);	
	auth_response = network_mysqld_auth_response_new();

	LUA_IMPORT_INT(auth_response, capabilities);
	LUA_IMPORT_INT(auth_response, max_packet_size);
	LUA_IMPORT_INT(auth_response, charset);

	LUA_IMPORT_STR(auth_response, username);
	LUA_IMPORT_STR(auth_response, response);
	LUA_IMPORT_STR(auth_response, database);

	if (network_mysqld_proto_append_auth_response(packet, auth_response)) {
		network_mysqld_auth_response_free(auth_response);

		luaL_error(L, "to_response_packet() failed");
		return 0;
	}

	lua_pushlstring(L, S(packet));
	
	network_mysqld_auth_response_free(auth_response);

	return 1;
}

static int lua_proto_get_challenge_packet (lua_State *L) {
	size_t packet_len;
	const char *packet_str = luaL_checklstring(L, 1, &packet_len);
	network_mysqld_auth_challenge *auth_challenge;
	network_packet packet;
	GString s;
	int err = 0;

	s.str = (char *)packet_str;
	s.len = packet_len;

	packet.data = &s;
	packet.offset = 0;

	auth_challenge = network_mysqld_auth_challenge_new();

	err = err || network_mysqld_proto_get_auth_challenge(&packet, auth_challenge);
	if (err) {
		network_mysqld_auth_challenge_free(auth_challenge);

		luaL_error(L, "%s: network_mysqld_proto_get_auth_challenge() failed", G_STRLOC);
		return 0;
	}

	lua_newtable(L);
	LUA_EXPORT_INT(auth_challenge, protocol_version);
	LUA_EXPORT_INT(auth_challenge, server_version);
	LUA_EXPORT_INT(auth_challenge, thread_id);
	LUA_EXPORT_INT(auth_challenge, capabilities);
	LUA_EXPORT_INT(auth_challenge, charset);
	LUA_EXPORT_INT(auth_challenge, server_status);

	LUA_EXPORT_STR(auth_challenge, challenge);

	network_mysqld_auth_challenge_free(auth_challenge);

	return 1;
}

static int lua_proto_append_challenge_packet (lua_State *L) {
	GString *packet;
	network_mysqld_auth_challenge *auth_challenge;

	luaL_checktype(L, 1, LUA_TTABLE);

	auth_challenge = network_mysqld_auth_challenge_new();

	LUA_IMPORT_INT(auth_challenge, protocol_version);
	LUA_IMPORT_INT(auth_challenge, server_version);
	LUA_IMPORT_INT(auth_challenge, thread_id);
	LUA_IMPORT_INT(auth_challenge, capabilities);
	LUA_IMPORT_INT(auth_challenge, charset);
	LUA_IMPORT_INT(auth_challenge, server_status);

	LUA_IMPORT_STR(auth_challenge, challenge);

	packet = g_string_new(NULL);	
	network_mysqld_proto_append_auth_challenge(packet, auth_challenge);

	lua_pushlstring(L, S(packet));
	
	network_mysqld_auth_challenge_free(auth_challenge);
	g_string_free(packet, TRUE);

	return 1;
}



/*
** Assumes the table is on top of the stack.
*/
static void set_info (lua_State *L) {
	lua_pushliteral (L, "_COPYRIGHT");
	lua_pushliteral (L, "Copyright (C) 2008 MySQL AB, 2008 Sun Microsystems, Inc");
	lua_settable (L, -3);
	lua_pushliteral (L, "_DESCRIPTION");
	lua_pushliteral (L, "export mysql protocol encoders and decoders mysql.*");
	lua_settable (L, -3);
	lua_pushliteral (L, "_VERSION");
	lua_pushliteral (L, "LuaMySQLProto 0.1");
	lua_settable (L, -3);
}


static const struct luaL_reg mysql_protolib[] = {
	{"from_err_packet", lua_proto_get_err_packet},
	{"to_err_packet", lua_proto_append_err_packet},
	{"from_ok_packet", lua_proto_get_ok_packet},
	{"to_ok_packet", lua_proto_append_ok_packet},
	{"from_eof_packet", lua_proto_get_eof_packet},
	{"to_eof_packet", lua_proto_append_eof_packet},
	{"from_challenge_packet", lua_proto_get_challenge_packet},
	{"to_challenge_packet", lua_proto_append_challenge_packet},
	{"from_response_packet", lua_proto_get_response_packet},
	{"to_response_packet", lua_proto_append_response_packet},
	{"from_masterinfo_string", lua_proto_get_masterinfo},
	{NULL, NULL},
};

#if defined(_WIN32)
# define LUAEXT_API __declspec(dllexport)
#else
# define LUAEXT_API extern
#endif

LUAEXT_API int luaopen_mysql_proto (lua_State *L) {
	luaL_register (L, "proto", mysql_protolib);
	set_info (L);
	return 1;
}
