#ifndef _LUA_SCOPE_H_
#define _LUA_SCOPE_H_

#include <glib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA_H
#include <lua.h>
#endif

typedef struct {
#ifdef HAVE_LUA_H
	lua_State *L;
	int L_ref;
#endif
#ifdef HAVE_GTHREAD
	GMutex *mutex;
#endif
	int L_top;
} lua_scope;

lua_scope *lua_scope_init(void);
void lua_scope_free(lua_scope *sc);

void lua_scope_get(lua_scope *sc);
void lua_scope_release(lua_scope *sc);

#ifdef HAVE_LUA_H
lua_State *lua_scope_load_script(lua_scope *sc, const gchar *name);
#endif

#endif
