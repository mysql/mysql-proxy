/* Copyright (C) 2008 MySQL AB */ 

#ifndef _QUERY_HANDLING_H_
#define _QUERY_HANDLING_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#ifdef HAVE_LUA_H
/**
 * embedded lua support
 */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif /* HAVE_LUA_H */

#include "network-exports.h"

typedef struct {
	/**
	 * the content of the OK packet 
	 */
	guint16 server_status;
	guint16 warning_count;
	guint64 affected_rows;
	guint64 insert_id;
    
	gboolean was_resultset;                      /**< if set, affected_rows and insert_id are ignored */
    
	/**
	 * MYSQLD_PACKET_OK or MYSQLD_PACKET_ERR
	 */	
	guint8 query_status;
} query_status;

typedef struct {
	GString *query;
    
	int id;                                 /**< a unique id set by the scripts to map the query to a handler */
    
	/* the userdata's need them */
	GQueue *result_queue;                   /**< the data to parse */
	query_status qstat;                     /**< summary information about the query status */
    
	GTimeVal ts_read_query;                 /**< timestamp when we added this query to the queues */
	GTimeVal ts_read_query_result_first;    /**< timestamp when we received the first packet */
	GTimeVal ts_read_query_result_last;     /**< timestamp when we received the last packet */

	guint64      rows;
	guint64      bytes;
} injection;

/**
 * parsed result set
 */
typedef struct {
	GQueue *result_queue;   /**< where the packets are read from */
    
	GPtrArray *fields;      /**< the parsed fields */
    
	GList *rows_chunk_head; /**< pointer to the EOF packet after the fields */
	GList *row;             /**< the current row */
    
	query_status qstat;     /**< state of this query */
	
	guint64      rows;
	guint64      bytes;
} proxy_resultset_t;

NETWORK_API injection *injection_init(int id, GString *query);
NETWORK_API void injection_free(injection *i);

NETWORK_API void proxy_getqueuemetatable(lua_State *L);
NETWORK_API void proxy_getinjectionmetatable(lua_State *L);

#endif /* _QUERY_HANDLING_H_ */
