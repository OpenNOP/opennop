#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h> // for multi-threading
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <termios.h>
#include <sys/time.h>

#include "clisocket.h"

void *fromserver_handler(void *dummyPtr) {
        int client_fd = *(int*)dummyPtr;
        int length;
        char server_reply[MAX_BUFFER_SIZE] = { 0 };

        while (((length = recv(client_fd, server_reply, MAX_BUFFER_SIZE, 0)) >= 0)
                        && (client_fd != 0)) {

                if (length > 0) {
                        server_reply[length] = '\0';
                        fprintf(stdout, "%s", server_reply);
                        fflush(stdout);
                } else {
                        if (length < 0)
                                perror("[cli_client]: recv");
                        else
                                //fprintf(stdout, "server closed connection\n");
                        exit(1);
                }
        }
        return 0;
}

void changemode(int dir) {

        static struct termios oldt, newt;
        if ( dir == 1 ) {
                tcgetattr( STDIN_FILENO, &oldt);
                newt = oldt;
                newt.c_lflag &= ~( ICANON | ECHO );
                tcsetattr( STDIN_FILENO, TCSANOW, &newt);
        } else {
                tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
        }
}

int kbhit (void) {
        struct timeval tv;
        fd_set rdfs;

        tv.tv_sec = 0;
        tv.tv_usec = 0;

        FD_ZERO(&rdfs);
        FD_SET (STDIN_FILENO, &rdfs);
        select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
        return FD_ISSET(STDIN_FILENO, &rdfs);
}

void append(char* s, char c) {
        int len = strlen(s);
        s[len] = c;
        s[len+1] = '\0';
}

int main(void) {
        int client_fd;
        int length;
        struct sockaddr_un server;
        char client_message[MAX_BUFFER_SIZE] = { 0 };
        char last_client_message[MAX_BUFFER_SIZE] = { 0 };
        pthread_t t_fromserver;
        int ch;

        if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
                perror("[cli_client]: socket");
                exit(1);
        }

        //fprintf(stdout, "\n Connecting to OpenNOP CLI... \n");
        server.sun_family = AF_UNIX;
        strcpy(server.sun_path, OPENNOP_SOCK);
        length = strlen(server.sun_path) + sizeof(server.sun_family);

        if (connect(client_fd, (struct sockaddr *) &server, length) == -1) {
                perror("[cli_client]: Connect");
                exit(1);
        }

        //fprintf(stdout, " Connected.....\n");

        pthread_create(&t_fromserver, NULL, fromserver_handler, (void *)&client_fd);

        // printf("\n opennopd# ");

        changemode(1);
        while(1) {
                ch = getchar();
                if(ch == 10) {

                        strcpy(last_client_message,client_message);
                        strcat(client_message,"\n");
                        fprintf(stdout,"\n");
                        if (send(client_fd, client_message, strlen(client_message), 0) == -1) {
                                perror("[cli_client]: send");
                                changemode(0);
                                exit(1);
                        }
                        client_message[0]= '\0';

                } else if(ch == 65) { //arrow up
                        //tbd - first char is missing
                        fprintf(stdout,"%s",last_client_message);
                        strcpy(client_message,last_client_message);

                } else if(ch == 9) { //tab key

                        //something should be send to server that should reply with possibilities?
                        //lets send just empty
                        fprintf(stdout,"\n");
                        send(client_fd, "help", 5, 0);
                } else if(ch == 127) { //backspace
                        //tbd - not working properly
                        fprintf(stdout,"\b");

                } else {
                        printf("%c", ch);
                        append(client_message,ch);

                }
        }
        changemode(0);

        pthread_join(t_fromserver, NULL);

        close(client_fd);
        return 0;
}

