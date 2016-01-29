#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "socket_utils.h"
#include "proxy_epoll.h"
#include "proxy_lua.h"
#include "proxy.h"

/* https://dev.mysql.com/doc/internals/en/mysql-packet.html
 * https://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::Handshake
 */
struct s_client_Packet
{
  uint16_t	payload_length_1;
  uint8_t	payload_length_2;
  uint8_t	sequence_id;
};

struct s_client_HandshakeResponse41
{
  struct s_client_Packet	payload_length;
  struct  {
    uint16_t			capabilities;
    uint16_t			capabilities_extended;
    uint32_t			max_packet_size;
    uint8_t			charset;
    unsigned char		reserved[23];
  } payload;
};

/* capabilities */
#define CLIENT_LONG_PASSWORD		(1 << 0)
#define CLIENT_PROTOCOL_41		(1 << 9)
#define CLIENT_SECURE_CONNECTION	(1 << 15)
#define CLIENT_CONNECT_WITH_DB		(1 << 3)

/* capabilities_extended */
#define CLIENT_PLUGIN_AUTH		(1 << 3)

static int f_cb_client_read(int fd_client, void *udata);
static int f_cb_backend_read(int fd_client, void *udata);

struct msg
{
  unsigned char buf_ptr[4000];
  unsigned int	buf_len;
};
static struct msg client_msg;
static struct msg backend_msg;

struct s_client_parsed_response
{
  unsigned char *username;
  unsigned char *db;
};

int do_proxy(int fd_client)
{
  char s_server_fake[] =
    "b\0\0"				/* payload length */
    "\0"				/* sequence id */
    "\n"				/* protocol version */
    "5.5.38-0ubuntu0.14.04.1folays1\0"	/* :-) */
    "\0\0\0\0"				/* connection ID */
    "xxxxxxxx\0"			/* auth-pluging-data-part-1 */
    "\377\367"				/* capability flags (lower 2 bytes) */
    "0"					/* character set */
    "\2\0"				/* status flag */
    "\17\200"				/* capability flags (upper 2 bytes) */
    "\25"				/* length of auth-plugin-data */
    "\0\0\0\0\0\0\0\0\0\0"		/* reserved */
    "xxxxxxxxxxxx\0"			/* auth-plugin-data-part-2 */
    "mysql_native_password\0"		/* auth_plugin-name */
    ;

  if (write(fd_client, s_server_fake, sizeof(s_server_fake) - 1) != sizeof(s_server_fake) - 1)
    err(1, "%s : write() to client", __func__);

  struct s_client_parsed_response d_client = (struct s_client_parsed_response){0};
  do_proxy_epoll(fd_client, f_cb_client_read, &d_client);

  proxy_lua_init();
  unsigned char *backend_host = NULL;
  unsigned char *backend_port = NULL;
  proxy_lua_exec(d_client.username, d_client.db, &backend_host, &backend_port);

  int fd_backend = socket_util_connect(backend_host, backend_port);
  do_proxy_epoll(fd_backend, f_cb_backend_read, NULL);

  if (write(fd_backend, client_msg.buf_ptr, client_msg.buf_len) != client_msg.buf_len)
    err(1, "%s : write() to backend", __func__);

  socket_util_forward(fd_client, fd_backend);
  return 0;
}

static int pkt_read(int fd, struct msg *msg)
{
  ssize_t nb_recv;
  nb_recv = recv(fd, msg->buf_ptr, sizeof(msg->buf_ptr), MSG_PEEK | MSG_DONTWAIT);
  if (nb_recv < 0)
    {
      switch (errno)
	{
	case ECONNRESET:
	  goto out_destroy;
	  break;
	case EAGAIN:
	  goto out;
	  break;
	default:
	  err(1, "%s : recv() return <0", __func__);
	  goto out_destroy;
	}
    }
  msg->buf_len = nb_recv;
  struct s_client_Packet *pkt = (void *)msg->buf_ptr;
  if (msg->buf_len < sizeof(*pkt))
    goto out;

  int len = (pkt->payload_length_2 << 16) + pkt->payload_length_1;
  if (len < sizeof(*pkt) || len > sizeof(msg->buf_ptr))
    goto out_destroy;

  if (msg->buf_len < len)
    goto out;

  if (recv(fd, msg->buf_ptr, msg->buf_len, 0) != msg->buf_len)
    goto out_destroy;

  return 1;

 out:
  return 0; /* message incomplete */

  out_destroy:
  return -1; /* message incorrect or error */
}

static int client_read_username(int fd_client, struct s_client_parsed_response *d_client)
{
  int ret = pkt_read(fd_client, &client_msg);
  if (ret != 1)
    return ret;

  struct s_client_Packet *pkt = (void *)&client_msg;
  if (pkt->sequence_id != 1)
    return -1;

  struct s_client_HandshakeResponse41 *hsr = (void *)&client_msg;
  if (client_msg.buf_len < sizeof(*hsr))
    return -1;

  if (!(hsr->payload.capabilities & CLIENT_PROTOCOL_41))
    return -1;
  if (!(hsr->payload.capabilities & CLIENT_LONG_PASSWORD))
    return -1;
  if (!(hsr->payload.capabilities & CLIENT_SECURE_CONNECTION))
    return -1;

  if (!(hsr->payload.capabilities_extended & CLIENT_PLUGIN_AUTH))
    return -1;

  unsigned char *username = (void *)&hsr[1];
  if (username > client_msg.buf_ptr + client_msg.buf_len - 1)
    return -1;
  void *username_null = memchr(username, '\0', client_msg.buf_len - (username - client_msg.buf_ptr));
  if (!username_null)
    return -1;

  d_client->username = strdup(username);

  uint8_t *password_length_ptr = username_null + 1;
  if (password_length_ptr > client_msg.buf_ptr + client_msg.buf_len - 1)
    return -1;

  if (hsr->payload.capabilities & CLIENT_CONNECT_WITH_DB)
    {
      unsigned char *db = password_length_ptr + sizeof(*password_length_ptr) + *password_length_ptr;
      if (db > client_msg.buf_ptr + client_msg.buf_len - 1)
	return -1;

      void *db_null = memchr(db, '\0', client_msg.buf_len - (db - client_msg.buf_ptr));
      if (!db_null)
	return -1;

      d_client->db = strdup(db);
    }

  return 1;
}

static int f_cb_client_read(int fd_client, void *udata)
{
  struct s_client_parsed_response *d_client = udata;

  int ret = client_read_username(fd_client, d_client);
  if (ret != 1)
    return ret;

  return 2;
}

static int f_cb_backend_read(int fd_client, void *udata)
{
  int ret = pkt_read(fd_client, &backend_msg);
  if (ret != 1)
    return ret;

  return 2;
}
