/* Copyright (C) 2008 MySQL AB
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */ 

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

typedef struct {
	/**
	 * the content of the OK packet 
	 */
	int server_status;
	int warning_count;
	guint64 affected_rows;
	guint64 insert_id;
    
	int was_resultset;                      /**< if set, affected_rows and insert_id are ignored */
    
	/**
	 * MYSQLD_PACKET_OK or MYSQLD_PACKET_ERR
	 */	
	int query_status;
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
} proxy_resultset_t;

injection *injection_init(int id, GString *query);
void injection_free(injection *i);

void proxy_getqueuemetatable(lua_State *L);
void proxy_getinjectionmetatable(lua_State *L);

#endif /* _QUERY_HANDLING_H_ */
