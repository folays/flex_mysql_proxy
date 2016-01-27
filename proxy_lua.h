#ifndef PROXY_LUA_H_
# define PROXY_LUA_H_

void proxy_lua_init(void);
void proxy_lua_exec(unsigned char *username, unsigned char **backend_host, unsigned char **backend_port);

#endif /* !PROXY_LUA_H_ */
