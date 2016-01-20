#ifndef SOCKET_UTILS_H_
# define SOCKET_UTILS_H_

int socket_util_listen(const char *addr, unsigned short port);
int socket_util_connect(const char *backend_host, const char *backend_port);
int socket_util_forward(int fd_backend, int fd_client);

#endif /* SOCKET_UTILS_H_ */
