#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void print_usage(FILE *stream) {
    fprintf(stream, "Usage: openport host port fd_num uid gid command [args...]\n");
}

int main(int argc, char **argv) {
    if(argc < 7) {
        print_usage(stderr);
        exit(2);
    }
    char *host = argv[1];
    int port = atoi(argv[2]);
    int fd = atoi(argv[3]);
    int uid = atoi(argv[4]);
    int gid = atoi(argv[5]);
    char **cmd = &argv[6];
    int ofd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(ofd < 0) { perror("Error creating socket"); exit(1); }
    int optval = 1;
    if(setsockopt(ofd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) { perror("Can't reuse"); exit(1); }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if(inet_aton(host, &addr.sin_addr) < 0) { perror("Bad address"); exit(1); }
    if(bind(ofd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("Can't bind"); exit(1); }
    if(listen(ofd, 128 << 10) < 0) { perror("Can't listen"); exit(1); }
    if(setgid(gid) < 0) { perror("Can't set gid"); exit(1); }
    if(setuid(uid) < 0) { perror("Can't set uid"); exit(1); }
    if(dup2(ofd, fd) < 0) { perror("Cant' dup"); exit(1); }
    if(execvp(cmd[0], cmd) < 0) { perror("Can't exec"); exit(127); }
    perror("Something wrong"); exit(127);
}
