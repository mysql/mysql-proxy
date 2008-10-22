#ifndef __NETWORK_MYSQLD_LUA__
#define __NETWORK_MYSQLD_LUA__

#include <lua.h>

#include "network-backend.h" /* query-status */
#include "network-injection.h" /* query-status */

#include "network-exports.h"

typedef enum {
	PROXY_NO_DECISION,
	PROXY_SEND_QUERY,
	PROXY_SEND_RESULT,
	PROXY_SEND_INJECTION,
	PROXY_IGNORE_RESULT       /** for read_query_result */
} network_mysqld_lua_stmt_ret;

NETWORK_API int network_mysqld_con_getmetatable(lua_State *L);
NETWORK_API void network_mysqld_lua_init_global_fenv(lua_State *L);

NETWORK_API void network_mysqld_lua_setup_global(lua_State *L, chassis_private *g);

typedef struct {
	struct {
		network_injection_queue *queries;       /**< queries we want to executed */
		query_status qstat;
		int sent_resultset;    /**< make sure we send only one result back to the client */
	} injected;

	lua_State *L;                  /**< lua state of the current connection */
	int L_ref;                     /**< reference into the lua_scope's registry */

	backend_t *backend;
	int backend_ndx;               /**< [lua] index into the backend-array */

	gboolean connection_close;     /**< [lua] set by the lua code to close a connection */

	struct timeval interval;       /**< the interval to be used for the timer */
	struct event evt_timer;        /**< the event structure used to implement the timer callback */

	gboolean is_reconnecting;      /**< if true, critical messages concerning failed connect() calls are suppressed, as they are expected errors */
} network_mysqld_con_lua_t;

NETWORK_API network_mysqld_con_lua_t *network_mysqld_con_lua_new();
NETWORK_API void network_mysqld_con_lua_free(network_mysqld_con_lua_t *st);

/** be sure to include network-mysqld.h */
NETWORK_API int network_mysqld_con_lua_register_callback(network_mysqld_con *con, const char *lua_script);
NETWORK_API int network_mysqld_con_lua_handle_proxy_response(network_mysqld_con *con, const char *lua_script);

#endif
