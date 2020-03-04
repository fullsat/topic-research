#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#define UNIXDOMAIN_PATH "/tmp/server.sock"

int main(int argc, char *argv[]){
    struct sockaddr_un srvaddr;
    int clientfd;

    clientfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(clientfd < 0){
        exit(-1);
    }

    memset(&srvaddr, 0, sizeof(struct sockaddr_un));
    srvaddr.sun_family = AF_UNIX;
    strcpy(srvaddr.sun_path, UNIXDOMAIN_PATH);

    if(connect(clientfd, (struct sockaddr *)&srvaddr, sizeof(struct sockaddr_un)) < 0){
        printf("connect() to failed: %s\n", strerror(errno));
        exit(-1);
    }

    struct timespec ts;
    memset(&ts, 0, sizeof(struct timespec));
    ts.tv_sec = 3;
    ts.tv_nsec = 0;
    printf("connect to socket\n");
    nanosleep(&ts, NULL); 

    close(clientfd);

    return 0;
}
