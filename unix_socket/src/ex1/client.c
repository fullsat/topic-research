#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

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
        exit(-1);
    }

    if(write(clientfd, "hello world", strlen("hello world")) < 0){
        exit(-1);
    }

    close(clientfd);

    return 0;
}
