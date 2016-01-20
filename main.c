#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "socket_utils.h"
#include "proxy.h"

int main(int argc, char **argv)
{
  int fd_frontend = socket_util_listen("0.0.0.0", PROXY_PORT);

  while (1)
    {
      int fd_client = accept(fd_frontend, NULL, NULL);

      int pid;

      if ((pid = fork()) == -1)
        return -1;
      if (pid)
        {
          /* parent */
          close(fd_client);
        }
      else
        {
          /* children */
	  close(fd_frontend);
          do_proxy(fd_client);
	  errx(1, "should not gere here");
	}
    }

  return 0;
}
