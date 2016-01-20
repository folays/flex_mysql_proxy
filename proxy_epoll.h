#ifndef PROXY_EPOLL_H_
# define PROXY_EPOLL_H_

typedef int (f_cb_socket_read)(int fd, void *udata);

void do_proxy_epoll(int fd_socket, f_cb_socket_read *f_read, void *udata);

#endif /* PROXY_EPOLL_H_ */
