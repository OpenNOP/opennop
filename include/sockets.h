#ifndef	_SOCKETS_H_
#define	_SOCKETS_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/epoll.h>

struct epoller;

typedef int (*t_epoll_callback)(struct epoller *, int, void *);
typedef int (*t_epoll_timeout)(struct epoller *);

/**
 *  @brief Create an instance of a epoller.
 *
 *  An epoller provides a quick way to setup an epoll instance.
 *  The epoller can be TCP or UDP have an associated server socket or only handle client sockets.
 *
 */
struct epoller {
	int socket;						/** Stores the server fd for client only set to 0.#socket */
	int epoll_fd;					/** Internal epoll instance used by this epoller.#epoll_fd */
	struct epoll_event event;		/** Internal event trigger.#event */
	struct epoll_event *events;		/** Internal pointer to list of events that need processed.#events */
	t_epoll_callback secure;		/** External pointer to function used to secure the connections.#secure */
	t_epoll_callback callback;  	/** External pointer to function used to process the messages.#callback */
	t_epoll_timeout timeoutfunction;/** External pointer to function used when epoller timesout.#timeoutfunction */
	int timeout;					/** Measure of time (ms) before epoll instance times out.#timeout */
};

int new_udp_client(__u32 serverip ,int port);
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
