#ifndef __SQL_TOKENIZER_LUA_H__
#define __SQL_TOKENIZER_LUA_H__

#include <lua.h>

#include "network-exports.h"

NETWORK_API int proxy_tokenize(lua_State *L);
NETWORK_API int sql_tokenizer_lua_getmetatable(lua_State *L);

#endif
