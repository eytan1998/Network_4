
#include <netinet/in.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 3000
#define BUFFER_SIZE 64


int main() {
    //===================
    // create tcp connection
    //===================
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("[-] watchdog: tcp socket creation failed.\n");
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))
        != 0) {
        printf("[-] watchdog: tcp connection with the server failed.\n");
        return -1;
    }

    //the timer
    //end - update every iteration, start - update every pet
    struct timeval start, end;
    gettimeofday(&start, 0);
    gettimeofday(&end, 0);

    char msg[BUFFER_SIZE] = {0};

    //loop until the timer pass 10 seconds
    while (1) {
        gettimeofday(&end, 0);

        bzero(msg, BUFFER_SIZE);
        //recv from ping: add - to keep the recv form blocking, pet - to reset the timer
        ssize_t e = recv(sockfd, msg, BUFFER_SIZE, 0);
        if (e == 0) break;
        if (e < 0) return -1;
        //the use of strstr because the  buffer is bigger and accept more than one command
        //get more than one command to prevent queue
        if (strstr(msg, "pet") != NULL) {
            gettimeofday(&start, 0);
        }

        time_t timeRemain = (10 - (end.tv_sec - start.tv_sec));
        printf("Massage from ping: %s, sec remaining %ld\n", msg, timeRemain);
        if (timeRemain <= 0) {
            //sending for ping to end "bark"
            send(sockfd, "bark", 4, 0);
            break;
        } else {
            //to prevent the ping recv blocking
            send(sockfd, "add", 3, 0);
        }

        sleep(1);
    }
    close(sockfd);

    return 0;
}
