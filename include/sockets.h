#ifndef	_SOCKETS_H_
#define	_SOCKETS_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

typedef int (*t_epoll_callback)(int fd, void *buf);

struct epoll_server {
	int socket;
	int epoll_fd;
	struct epoll_event event;
	struct epoll_event *events;
	t_epoll_callback callback;
};

int new_ip_client(__u32 serverip ,int port);
int new_ip_server(int port);
int accept_ip_client(int server_socket);
int new_unix_client(char* path);
int new_unix_server(char* path);
int accept_unix_client(int server_socket);
int make_socket_non_blocking (int socket);
int register_socket(int listener_socket, int epoll_fd, struct epoll_event *event);
int new_ip_epoll_server(struct epoll_server *server, t_epoll_callback callback, int port);
int shutdown_epoll_server(struct epoll_server *server);
int epoll_handler(struct epoll_server *server);

#endif
