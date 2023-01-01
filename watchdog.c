#include <stdio.h>
#include <sys/time.h> // gettimeofday()
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()

#define PORT 3000

#include <signal.h>
#include <stdlib.h>


int main() {
    int sockfd;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    } else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    } else
        printf("connected to the server..\n");

    struct timeval start, end;
    gettimeofday(&start, 0);
    gettimeofday(&end, 0);


    char msg[4] = {0};
    int status;

    while (10 - (end.tv_sec - start.tv_sec) > 0) {
        int e = recv(sockfd, msg, 4, 0);
        if (e == 0) break;
        if (e < 0) return -1;
        if (strcmp(msg, "pet") == 0) {
            gettimeofday(&start, 0);
        }
        gettimeofday(&end, 0);
    //    printf("msg = %s,time for time out: %ld\n", msg, 10 - (end.tv_sec - start.tv_sec));

        strcpy(msg,"add\0");
        send(sockfd, msg, 4, 0);
        bzero(msg, 4);
    }
    printf("Sending end msg\n");
    bzero(msg, 4);
    strcpy(msg,"end\0");
    send(sockfd, msg, 4, 0);
    close(sockfd);
    return 0;
}
