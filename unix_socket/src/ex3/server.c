#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <time.h>

#define UNIXDOMAIN_PATH "/tmp/server.sock"

int main() {

  int listenfd, estafd;
  struct sockaddr_un srvaddr, cliaddr;
  char recvbuf[256];


  listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(listenfd < 0){
    exit(255);
  }

  unlink(UNIXDOMAIN_PATH);
  memset(&srvaddr, 0, sizeof(struct sockaddr_un));

  srvaddr.sun_family = AF_UNIX;
  strcpy(srvaddr.sun_path, UNIXDOMAIN_PATH);
  if(bind(listenfd, (struct sockaddr *)&srvaddr, sizeof(struct sockaddr_un)) < 0){
    exit(255);
  }

  if(listen(listenfd, 5) < 0) {
    exit(255);
  }

  memset(&cliaddr, 0, sizeof(struct sockaddr_un));
  socklen_t addrlen = sizeof(struct sockaddr_un);
  if((estafd = accept(listenfd, (struct sockaddr *)&cliaddr, &addrlen)) < 0){
    exit(255);
  }

  int len = read(estafd, recvbuf, sizeof(recvbuf));
  printf("%s\n", recvbuf);

  close(listenfd);
  close(estafd);

  return 0;
}
