NAME	= flex_mysql_proxy
CC	= gcc
RM	= rm -f

debug		= 0
proxy_port	= 3307

.if defined(proxy_port)
PROXY_PORT	= $(proxy_port)
.endif

LUA_CFLAGS	!= pkg-config --cflags lua5.2
LUA_LIBS	!= pkg-config --libs lua5.2

CFLAGS		+= $(LUA_CFLAGS) -g -ggdb -Werror -fPIC -DPROXY_PORT=$(proxy_port) -DFLEX_PROXY_VERBOSE=$(debug)
CFLAGS		+= -Wmissing-prototypes -Wimplicit-function-declaration
LIBS		= $(LUA_LIBS)

SRC		= main.c socket_utils.c proxy_epoll.c proxy.c proxy_lua.c
OBJ		= $(SRC:.c=.o)

all	: $(NAME)

re	: fclean all

clean	:
	-$(RM) $(OBJ) $(OBJ) *~

fclean	: clean
	-$(RM) $(NAME)
	-$(RM) *.o

$(NAME)	: $(OBJ)
	$(CC) -o $(NAME) $(.ALLSRC) $(LIBS)
