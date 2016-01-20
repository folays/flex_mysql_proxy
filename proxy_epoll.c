#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <sys/epoll.h>

#include "proxy_epoll.h"

#define MAX_EPOLL	1

void do_proxy_epoll(int fd_socket, f_cb_socket_read *f_read, void *udata)
{
  struct epoll_event events[MAX_EPOLL];

  int fd_epoll = epoll_create(MAX_EPOLL);
  if (fd_epoll < 0)
    err(1, "%s : epoll_create", __func__);
  {
    struct epoll_event ev = (struct epoll_event){.events = EPOLLET | EPOLLIN | EPOLLRDHUP, .data = NULL};
    if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_socket, &ev) != 0)
      err(1, "%s : epoll_ctl", __func__);
  }

  while (1)
    {
      int nfds, i;

      nfds = epoll_wait(fd_epoll, events, MAX_EPOLL, -1);
      if (nfds == -1 && errno != EINTR)
	err(1, "%s : epoll_wait", __func__);

      for (i = 0; i < nfds; ++i)
	{
	  struct epoll_event *event = &events[i];

	  {
	    int ret = -1;
	    do
	      {
		ret = f_read(fd_socket, udata);
	      }
	    while (ret == 1);

	    if (ret < 0 || event->events & EPOLLRDHUP)
	      exit(1);
	    if (ret == 2)
	      goto epoll_break;
	  }
	}
    }

 epoll_break:
  {
    struct epoll_event ev = {0};
    if (epoll_ctl(fd_epoll, EPOLL_CTL_DEL, fd_socket, &ev) != 0)
      err(1, "%s : epoll_ctl", __func__);
  }

  if (close(fd_epoll) != 0)
    err(1, "%s : close", __func__);
}
