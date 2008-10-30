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
#ifndef __LUA_ENV_H__
#define __LUA_ENV_H__

#include <sys/types.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "network-exports.h"

NETWORK_API void lua_getfield_literal (lua_State *L, int idx, const char *k, size_t k_len);
NETWORK_API void *luaL_checkself (lua_State *L);
NETWORK_API int proxy_getmetatable(lua_State *L, const luaL_reg *methods);

#endif
