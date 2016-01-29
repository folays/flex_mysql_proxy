#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "proxy_lua.h"

static lua_State *proxy_L;

void proxy_lua_init(void)
{
  proxy_L = luaL_newstate();
}

void proxy_lua_exec(unsigned char *username, unsigned char *db, unsigned char **backend_host, unsigned char **backend_port)
{
  lua_State *L = proxy_L;

  luaL_openlibs(L);

  if (luaL_loadfile(L, "/etc/flex_mysql_proxy/scripts/proxy.lua"))
    luaL_error(L, "%s:%d %s pcall error : %s", __FILE__, __LINE__, __func__, lua_tostring(L, -1));
  if (lua_pcall(L, 0, 0, 0))
    luaL_error(L, "%s:%d %s pcall error : %s", __FILE__, __LINE__, __func__, lua_tostring(L, -1));

  lua_getglobal(L, "get_backend_from_username");
  lua_pushstring(L, username);
  {
    if (db)
      lua_pushstring(L, db);
    else
      lua_pushnil(L);
  }
  if (lua_pcall(L, 2, 2, 0))
    luaL_error(L, "%s:%d %s pcall error : %s", __FILE__, __LINE__, __func__, lua_tostring(L, -1));

  *backend_host = strdup(lua_tostring(L, -2));
  *backend_port = strdup(lua_tostring(L, -1));

  lua_pop(L, 2);

  lua_close(L);
}
