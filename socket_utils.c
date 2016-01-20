#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>

#include "socket_utils.h"

int socket_util_listen(const char *addr, unsigned short port)
{
  int s;
  struct sockaddr_in name;
  int namelen;
  int reuse = 1;

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    err(1, "%s : socket", __func__);
  fcntl(s, F_SETFD, fcntl(s, F_GETFD, NULL) | FD_CLOEXEC);
  namelen = sizeof(struct sockaddr);
  name.sin_family = AF_INET;
  name.sin_port = htons(port);
  name.sin_addr.s_addr = inet_addr(addr);
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (int *)&reuse, sizeof(reuse));
  if (bind(s, (struct sockaddr *)&name, namelen) != 0)
    err(1, "%s : bind", __func__);
  if (listen(s, 512) != 0)
    err(1, "%s : listen", __func__);
  return s;
}

int socket_util_connect(const char *backend_host, const char *backend_port)
{
  int gaierr;
  struct addrinfo hints, *aitop;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = 0;
  gaierr = getaddrinfo(backend_host, backend_port, &hints, &aitop);
  if (gaierr)
    errx(1, "%s : getaddrinfo error: %s", __func__, gai_strerror(gaierr));

  int fd_backend = socket(aitop->ai_family, aitop->ai_socktype, aitop->ai_protocol);

  if (connect(fd_backend, aitop->ai_addr, aitop->ai_addrlen))
    err(1, "%s : connect failed", __func__);
  freeaddrinfo(aitop);

  return fd_backend;
}

static void socket_util_splice(int fd_in, int fd_out, int pfd[2], int pipe_size)
{
  int bytes;

  bytes = splice(fd_in, NULL, pfd[1], NULL, pipe_size, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
  if (bytes < 0)
    err(1, "flex %s %s : splice input failed", __FILE__, __func__);
  /* intentional blocking:
   * - we should not return until all data is wrote otherwise nothing make us retry to flush it
   *   (because the output data would be stuck in the pipe and we would not retry to splice it to fd_out)
   */
  bytes = splice(pfd[0], NULL, fd_out, NULL, pipe_size, SPLICE_F_MOVE);
  if (bytes < 0)
    err(1, "flex %s %s : splice output failed", __FILE__, __func__);
}

int socket_util_forward(int fd_backend, int fd_client)
{
  struct pollfd fds[2];

  int pfd_from_client[2], pfd_from_backend[2];
  if ((pipe(pfd_from_client) != 0) || (pipe(pfd_from_backend) != 0))
    err(1, "flex %s %s : pipe failed", __FILE__, __func__);
  int pipe_from_client_size = fcntl(pfd_from_client[1], F_GETPIPE_SZ);
  int pipe_from_backend_size = fcntl(pfd_from_backend[1], F_GETPIPE_SZ);
  if (pipe_from_client_size <= 0 || pipe_from_backend_size <= 0)
    err(1, "flex %s %s : F_GETPIPE_SZ failed", __FILE__, __func__);

  fds[0] = (struct pollfd){.fd = fd_client, .events = POLLIN | POLLRDHUP};
  fds[1] = (struct pollfd){.fd = fd_backend, .events = POLLIN | POLLRDHUP};

  //printf("flex %s %s : entering forward mode...\n", __FILE__, __func__);
  while (1)
    {
      //printf("flex %s %s : poll...\n", __FILE__, __func__);
      int nb_mod = poll(fds, sizeof(fds)/sizeof(*fds), -1);
      //printf("flex %s %s : poll returned %d\n", __FILE__, __func__, nb_mod);

      if (fds[0].revents & (POLLIN | POLLRDHUP))
        {
          socket_util_splice(fds[0].fd, fds[1].fd, pfd_from_client, pipe_from_client_size);
        }
      if (fds[1].revents & (POLLIN | POLLRDHUP))
        {
          socket_util_splice(fds[1].fd, fds[0].fd, pfd_from_backend, pipe_from_backend_size);
        }
    }
}
