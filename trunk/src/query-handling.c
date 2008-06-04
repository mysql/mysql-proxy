/* Copyright (C) 2008 MySQL AB */ 

#include <string.h>

#include "query-handling.h"

#include "network-mysqld-proto.h"
#include "network-mysqld-packet.h"
#include "glib-ext.h"

#define C(x) x, sizeof(x) - 1
#define S(x) x->str, x->len

#define TIME_DIFF_US(t2, t1) \
((t2.tv_sec - t1.tv_sec) * 1000000.0 + (t2.tv_usec - t1.tv_usec))


/**
 * emulate luaL_newmetatable() with lightuserdata instead of strings
 */
void proxy_getmetatable(lua_State *L, const luaL_reg *methods) {
	lua_pushlightuserdata(L, (luaL_reg *)methods);
	lua_gettable(L, LUA_REGISTRYINDEX);
    
	if (lua_isnil(L, -1)) {
		/* not found */
		lua_pop(L, 1);
        
		lua_newtable(L);
		luaL_register(L, NULL, methods);
        
		lua_pushlightuserdata(L, (luaL_reg *)methods);
		lua_pushvalue(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	}
	g_assert(lua_istable(L, -1));
}

/**
 * Initialize an injection struct.
 */
injection *injection_init(int id, GString *query) {
	injection *i;
    
	i = g_new0(injection, 1);
	i->id = id;
	i->query = query;
    
	/**
	 * we have to assume that injection_init() is only used by the read_query call
	 * which should be fine
	 */
	g_get_current_time(&(i->ts_read_query));
    
	return i;
}

/**
 * Free an injection struct
 */
void injection_free(injection *i) {
	if (!i) return;
    
	if (i->query) g_string_free(i->query, TRUE);
    
	g_free(i);
}

/**
 * check pass through the userdata as is
 */
static void *luaL_checkself (lua_State *L) {
    /** @todo: actually check here */
	return lua_touserdata(L, 1);
}

static int proxy_queue_append(lua_State *L) {
	/* we expect 2 parameters */
	GQueue *q = *(GQueue **)luaL_checkself(L);
	int resp_type = luaL_checkinteger(L, 2);
	size_t str_len;
	const char *str = luaL_checklstring(L, 3, &str_len);
    
	GString *query = g_string_sized_new(str_len);
	g_string_append_len(query, str, str_len);
    
	g_queue_push_tail(q, injection_init(resp_type, query));
    
	return 0;
}

static int proxy_queue_prepend(lua_State *L) {
	/* we expect 2 parameters */
	GQueue *q = *(GQueue **)luaL_checkself(L);
	int resp_type = luaL_checkinteger(L, 2);
	size_t str_len;
	const char *str = luaL_checklstring(L, 3, &str_len);
    
	GString *query = g_string_sized_new(str_len);
	g_string_append_len(query, str, str_len);
    
	g_queue_push_head(q, injection_init(resp_type, query));
    
	return 0;
}

static int proxy_queue_reset(lua_State *L) {
	/* we expect 2 parameters */
	GQueue *q = *(GQueue **)luaL_checkself(L);
	injection *inj;
    
	while ((inj = g_queue_pop_head(q))) injection_free(inj);
    
	return 0;
}

static int proxy_queue_len(lua_State *L) {
	/* we expect 2 parameters */
	GQueue *q = *(GQueue **)luaL_checkself(L);
    
	lua_pushinteger(L, q->length);
    
	return 1;
}

static const struct luaL_reg methods_proxy_queue[] = {
	{ "prepend", proxy_queue_prepend },
	{ "append", proxy_queue_append },
	{ "reset", proxy_queue_reset },
	{ "__len", proxy_queue_len },
	{ NULL, NULL },
};

/**
 * Push a metatable onto the Lua stack containing methods to 
 * handle the query injection queue.
 */
void proxy_getqueuemetatable(lua_State *L) {
    proxy_getmetatable(L, methods_proxy_queue);
}

/**
 * Initialize a resultset struct
 */
proxy_resultset_t *proxy_resultset_init() {
	proxy_resultset_t *res;
    
	res = g_new0(proxy_resultset_t, 1);
    
	return res;
}

/**
 * Free a resultset struct
 */
void proxy_resultset_free(proxy_resultset_t *res) {
	if (!res) return;
    
	if (res->fields) {
		network_mysqld_proto_fielddefs_free(res->fields);
	}
    
	g_free(res);
}

/**
 * Free a resultset struct when the corresponding Lua userdata is garbage collected.
 */
static int proxy_resultset_gc(lua_State *L) {
	proxy_resultset_t *res = *(proxy_resultset_t **)lua_touserdata(L, 1);
	
	proxy_resultset_free(res);
    
	return 0;
}

/**
 * Free a resultset struct when the corresponding Lua userdata is garbage collected.
 */
static int proxy_resultset_gc_light(lua_State *L) {
	proxy_resultset_t *res = *(proxy_resultset_t **)lua_touserdata(L, 1);
	
	g_free(res);
    
	return 0;
}

static int proxy_resultset_fields_len(lua_State *L) {
	GPtrArray *fields = *(GPtrArray **)luaL_checkself(L);
    lua_pushinteger(L, fields->len);
    return 1;
}

static int proxy_resultset_field_get(lua_State *L) {
	MYSQL_FIELD *field = *(MYSQL_FIELD **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);
        
	if (strleq(key, keysize, C("type"))) {
		lua_pushinteger(L, field->type);
	} else if (strleq(key, keysize, C("name"))) {
		lua_pushstring(L, field->name);
	} else if (strleq(key, keysize, C("org_name"))) {
		lua_pushstring(L, field->org_name);
	} else if (strleq(key, keysize, C("org_table"))) {
		lua_pushstring(L, field->org_table);
	} else if (strleq(key, keysize, C("table"))) {
		lua_pushstring(L, field->table);
	} else {
		lua_pushnil(L);
	}
    
	return 1;
}

static const struct luaL_reg methods_proxy_resultset_fields_field[] = {
	{ "__index", proxy_resultset_field_get },
	{ NULL, NULL },
};

/**
 * get a field from the result-set
 *
 */
static int proxy_resultset_fields_get(lua_State *L) {
	GPtrArray *fields = *(GPtrArray **)luaL_checkself(L);
	MYSQL_FIELD *field;
	MYSQL_FIELD **field_p;
	lua_Integer ndx = luaL_checkinteger(L, 2);

	/* protect the compare */
	if (fields->len > G_MAXINT) {
		return 0;
	}
    
	if (ndx < 1 || ndx > (lua_Integer)fields->len) {
		lua_pushnil(L);
        
		return 1;
	}
    
	field = fields->pdata[ndx - 1]; /** lua starts at 1, C at 0 */
    
	field_p = lua_newuserdata(L, sizeof(field));
	*field_p = field;
    
	proxy_getmetatable(L, methods_proxy_resultset_fields_field);
	lua_setmetatable(L, -2);
    
	return 1;
}

/**
 * get the next row from the resultset
 *
 * returns a lua-table with the fields (starting at 1)
 *
 * @return 0 on error, 1 on success
 *
 */
static int proxy_resultset_rows_iter(lua_State *L) {
	proxy_resultset_t *res = *(proxy_resultset_t **)lua_touserdata(L, lua_upvalueindex(1));
	network_packet packet;
	GPtrArray *fields = res->fields;
	gsize i;
	gint8 status;
    
	GList *chunk = res->row;
    
	if (chunk == NULL) return 0;

	packet.data = chunk->data;
	packet.offset = 0;

	network_mysqld_proto_skip_network_header(&packet);

	status = network_mysqld_proto_get_int8(&packet);
    
	/* if we find the 2nd EOF packet we are done */
	if (status == MYSQLD_PACKET_EOF &&
	    packet.data->len < 10) {
		return 0;
	}
    
	/* a ERR packet instead of real rows
	 *
	 * like "explain select fld3 from t2 ignore index (fld3,not_existing)"
	 *
	 * see mysql-test/t/select.test
	 *  */
	if (status == MYSQLD_PACKET_ERR) {
		return 0;
	}

	packet.offset--; /* well, either it is ERR, EOF or a length-encoded field-length, parse it again */

	lua_newtable(L);
    
	for (i = 0; i < fields->len; i++) {
		guint64 field_len;
        
		field_len = network_mysqld_proto_get_lenenc_int(&packet);

		if (field_len == 251) { /** @todo use constant */
			lua_pushnil(L);
		} else {
			/**
			 * @todo we only support fields in the row-iterator < 16M (packet-len)
			 */
			g_assert_cmpint(field_len, <=, packet.data->len);
			g_assert_cmpint(packet.offset + field_len, <=, packet.data->len);
            
			lua_pushlstring(L, packet.data->str + packet.offset, field_len);

			network_mysqld_proto_skip(&packet, field_len);
		}
        
		/* lua starts its tables at 1 */
		lua_rawseti(L, -2, i + 1);
	}
    
	res->row = res->row->next;
    
	return 1;
}

/**
 * parse the result-set of the query
 *
 * @return if this is not a result-set we return -1
 */
static int parse_resultset_fields(proxy_resultset_t *res) {
	GString *packet = res->result_queue->head->data;
	GList *chunk;
    
	if (res->fields) return 0;
    
	switch (packet->str[NET_HEADER_SIZE]) {
        case MYSQLD_PACKET_OK:
        case MYSQLD_PACKET_ERR:
            res->qstat.query_status = packet->str[NET_HEADER_SIZE];
            
            return 0;
        default:
            /* OK with a resultset */
            res->qstat.query_status = MYSQLD_PACKET_OK;
            break;
	}
    
	/* parse the fields */
	res->fields = network_mysqld_proto_fielddefs_new();
    
	if (!res->fields) return -1;
    
	chunk = network_mysqld_proto_get_fielddefs(res->result_queue->head, res->fields);
    
	/* no result-set found */
	if (!chunk) return -1;
    
	/* skip the end-of-fields chunk */
	res->rows_chunk_head = chunk->next;
    
	return 0;
}

static const struct luaL_reg methods_proxy_resultset_fields[] = {
	{ "__index", proxy_resultset_fields_get },
	{ "__len", proxy_resultset_fields_len },
	{ NULL, NULL },
};

static const struct luaL_reg methods_proxy_resultset_light[] = {
	{ "__gc", proxy_resultset_gc_light },
	{ NULL, NULL },
};

static int proxy_resultset_get(lua_State *L) {
	proxy_resultset_t *res = *(proxy_resultset_t **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);
    
	if (strleq(key, keysize, C("fields"))) {
		GPtrArray **fields_p;
        
		parse_resultset_fields(res);
        
		if (res->fields) {
			fields_p = lua_newuserdata(L, sizeof(res->fields));
			*fields_p = res->fields;
            
			proxy_getmetatable(L, methods_proxy_resultset_fields);
			lua_setmetatable(L, -2); /* tie the metatable to the table   (sp -= 1) */
		} else {
			lua_pushnil(L);
		}
	} else if (strleq(key, keysize, C("rows"))) {
		proxy_resultset_t *rows;
		proxy_resultset_t **rows_p;
        
		parse_resultset_fields(res);
        
		if (res->rows_chunk_head) {
            
			rows = proxy_resultset_init();
			rows->rows_chunk_head = res->rows_chunk_head;
			rows->row    = rows->rows_chunk_head;
			rows->fields = res->fields;
            
			/* push the parameters on the stack */
			rows_p = lua_newuserdata(L, sizeof(rows));
			*rows_p = rows;
            
			proxy_getmetatable(L, methods_proxy_resultset_light);
			lua_setmetatable(L, -2);
            
			/* return a interator */
			lua_pushcclosure(L, proxy_resultset_rows_iter, 1);
		} else {
			lua_pushnil(L);
		}
	} else if (strleq(key, keysize, C("row_count"))) {
		lua_pushinteger(L, res->rows);
	} else if (strleq(key, keysize, C("bytes"))) {
		lua_pushinteger(L, res->bytes);
	} else if (strleq(key, keysize, C("raw"))) {
		GString *s = res->result_queue->head->data;
		lua_pushlstring(L, s->str + 4, s->len - 4);
	} else if (strleq(key, keysize, C("flags"))) {
		lua_newtable(L);
		lua_pushboolean(L, (res->qstat.server_status & SERVER_STATUS_IN_TRANS) != 0);
		lua_setfield(L, -2, "in_trans");
        
		lua_pushboolean(L, (res->qstat.server_status & SERVER_STATUS_AUTOCOMMIT) != 0);
		lua_setfield(L, -2, "auto_commit");
		
		lua_pushboolean(L, (res->qstat.server_status & SERVER_QUERY_NO_GOOD_INDEX_USED) != 0);
		lua_setfield(L, -2, "no_good_index_used");
		
		lua_pushboolean(L, (res->qstat.server_status & SERVER_QUERY_NO_INDEX_USED) != 0);
		lua_setfield(L, -2, "no_index_used");
	} else if (strleq(key, keysize, C("warning_count"))) {
		lua_pushinteger(L, res->qstat.warning_count);
	} else if (strleq(key, keysize, C("affected_rows"))) {
		/**
		 * if the query had a result-set (SELECT, ...) 
		 * affected_rows and insert_id are not valid
		 */
		if (res->qstat.was_resultset) {
			lua_pushnil(L);
		} else {
			lua_pushnumber(L, res->qstat.affected_rows);
		}
	} else if (strleq(key, keysize, C("insert_id"))) {
		if (res->qstat.was_resultset) {
			lua_pushnil(L);
		} else {
			lua_pushnumber(L, res->qstat.insert_id);
		}
	} else if (strleq(key, keysize, C("query_status"))) {
		if (0 != parse_resultset_fields(res)) {
			/* not a result-set */
			lua_pushnil(L);
		} else {
			lua_pushinteger(L, res->qstat.query_status);
		}
	} else {
		lua_pushnil(L);
	}
    
	return 1;
}

static const struct luaL_reg methods_proxy_resultset[] = {
	{ "__index", proxy_resultset_get },
	{ "__gc", proxy_resultset_gc },
	{ NULL, NULL },
};

static int proxy_injection_get(lua_State *L) {
	injection *inj = *(injection **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);
    
	if (strleq(key, keysize, C("type"))) {
		lua_pushinteger(L, inj->id); /** DEPRECATED: use "inj.id" instead */
	} else if (strleq(key, keysize, C("id"))) {
		lua_pushinteger(L, inj->id);
	} else if (strleq(key, keysize, C("query"))) {
		lua_pushlstring(L, inj->query->str, inj->query->len);
	} else if (strleq(key, keysize, C("query_time"))) {
		lua_pushinteger(L, TIME_DIFF_US(inj->ts_read_query_result_first, inj->ts_read_query));
	} else if (strleq(key, keysize, C("response_time"))) {
		lua_pushinteger(L, TIME_DIFF_US(inj->ts_read_query_result_last, inj->ts_read_query));
	} else if (strleq(key, keysize, C("resultset"))) {
		/* fields, rows */
		proxy_resultset_t *res;
		proxy_resultset_t **res_p;
        
		res_p = lua_newuserdata(L, sizeof(res));
		*res_p = res = proxy_resultset_init();
        
		res->result_queue = inj->result_queue;
		res->qstat = inj->qstat;
		res->rows  = inj->rows;
		res->bytes = inj->bytes;
        
		proxy_getmetatable(L, methods_proxy_resultset);
		lua_setmetatable(L, -2);
	} else {
		g_message("%s.%d: inj[%s] ... not found", __FILE__, __LINE__, key);
        
		lua_pushnil(L);
	}
    
	return 1;
}

static const struct luaL_reg methods_proxy_injection[] = {
	{ "__index", proxy_injection_get },
	{ NULL, NULL },
};

/**
 * Push a metatable onto the Lua stack containing methods to 
 * access the resultsets.
 */
void proxy_getinjectionmetatable(lua_State *L) {
    proxy_getmetatable(L, methods_proxy_injection);
}
