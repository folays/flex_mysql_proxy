NAME	= flex_mysql_proxy
CC	= gcc
RM	= rm -f

debug		= 0
proxy_port	= 3307
proxy_zone	= %s.sql.example.net

.if defined(proxy_port)
PROXY_PORT	= $(proxy_port)
.endif

.if defined(proxy_zone)
PROXY_ZONE	= $(proxy_zone)
.endif

CFLAGS		+=  -g -ggdb -Werror -fPIC -DPROXY_PORT=$(proxy_port) -DPROXY_ZONE=\"$(proxy_zone)\" -DFLEX_PROXY_VERBOSE=$(debug)
CFLAGS		+= -Wmissing-prototypes -Wimplicit-function-declaration
#LIBS		= -lpthread

SRC		= main.c socket_utils.c proxy_epoll.c proxy.c
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
