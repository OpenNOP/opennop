#ifndef	_SOCKETS_H_
#define	_SOCKETS_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

struct epoller;

typedef int (*t_epoll_callback)(struct epoller *, int, void *);
typedef int (*t_epoll_timeout)(struct epoller *);

struct epoller {
	int socket;
	int epoll_fd;
	struct epoll_event event;
	struct epoll_event *events;
	t_epoll_callback secure;
	t_epoll_callback callback;
	t_epoll_timeout timeoutfunction;
	int timeout;
};

int new_ip_client(__u32 serverip ,int port);
int new_ip_server(int port);
int accept_ip_client(int server_socket);
int new_unix_client(char* path);
int new_unix_server(char* path);
int accept_unix_client(int server_socket);
int make_socket_non_blocking (int socket);
int register_socket(int listener_socket, int epoll_fd, struct epoll_event *event);
int new_ip_epoll_server(struct epoller *server, t_epoll_callback secure, t_epoll_callback callback, int port, t_epoll_timeout timeoutfunction, int timeout);
int shutdown_epoll_server(struct epoller *server);
int epoll_handler(struct epoller *server);

#endif
