#ifndef	_SOCKETS_H_
#define	_SOCKETS_H_

#include <linux/types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

int new_ip_client(__u32 serverip ,int port);
int new_ip_server(int port);
int accept_ip_client(int server_socket);
int new_unix_client(char* path);
int new_unix_server(char* path);
int accept_unix_client(int server_socket);
int make_socket_non_blocking (int socket);

#endif
