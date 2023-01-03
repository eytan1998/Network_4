
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>


#define ICMP_HDR_LEN 8
#define BUFFER_SIZE  64
#define DEFAULT_DESTINATION_IP "8.8.8.8"


int execute_and_connectTo_watch_dog() {
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("[-] ping: tcp socket creation failed.\n");
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(3000);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
        printf("[-] ping: tcp socket bind failed.\n");
        return -1;
    }

    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) {
        printf("[-] ping: tcp Listen failed.\n");
        return -1;
    }

    //exec watchdog after "server" is listening
    char *args[2];
    args[0] = "./watchdog";
    args[1] = NULL;
    int pid = fork();
    if (pid == 0) {
        execvp(args[0], args);
    }


    // Accept the data packet from client and verification
    int len = sizeof(cli);
    connfd = accept(sockfd, (struct sockaddr *) &cli, &len);
    if (connfd < 0) {
        printf("[-] ping: tcp server accept failed.\n");
        return -1;
    }

    close(sockfd);
    return connfd;
}

int chat_with_watchdog(int sock_watchdog, char *dest_ip) {
    char msg[BUFFER_SIZE];
    while (1) {
        //to prevent the watchdog recv blocking
        send(sock_watchdog, "add", 3, 0);

        //recv from ping: add - to keep the recv form blocking, bark - close program
        bzero(msg, BUFFER_SIZE);
        recv(sock_watchdog, msg, BUFFER_SIZE, 0);
        //the use of strstr because the  buffer is bigger and accept more than one command
        //get more than one command to prevent queue

        printf("Massage from watchdog: %s\n", msg);

        if (strstr(msg, "bark") != NULL) {
            printf("[-] Server %s cannot be reached.\n", dest_ip);
            return -1;
        }
        sleep(1);
    }


}

// Compute checksum (RFC 1071).
unsigned short calculate_checksum(unsigned short *paddress, int len) {
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *((unsigned char *) &answer) = *((unsigned char *) w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}

// Check if given if is valid
int check_invalid_address(char *address) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, address, &(sa.sin_addr));
    return result == 0;
}

int send_pings(int sock, int sock_watchdog, char *dest_ip) {

    struct timeval start, end;
    struct icmp icmphdr; // ICMP-header
    int seq_counter = 0;
    char data[] = "i am ping!\0";
    unsigned int datalen = strlen(data) + 1;

    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;
    // The port is irrelevant for Networking and therefore was zeroed.
    dest_in.sin_addr.s_addr = inet_addr(dest_ip);

    while (1) {

        //===================
        // ICMP header
        //===================

        // Message Type (8 bits): ICMP_ECHO_REQUEST
        icmphdr.icmp_type = ICMP_ECHO;

        // Message Code (8 bits): echo request
        icmphdr.icmp_code = 0;

        // Identifier (16 bits): some number to trace the response.
        // It will be copied to the response packet and used to map response to the request sent earlier.
        // Thus, it serves as a Transaction-ID when we need to make "ping"
        icmphdr.icmp_id = 18;

        // Sequence Number (16 bits): starts at 0
        icmphdr.icmp_seq = seq_counter++;

        // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
        icmphdr.icmp_cksum = 0;

        //===================
        // Combine the packet
        //===================
        char packet[IP_MAXPACKET];

        // Next, ICMP header
        memcpy((packet), &icmphdr, ICMP_HDR_LEN);

        // After ICMP header, add the ICMP data.
        memcpy(packet + ICMP_HDR_LEN, data, datalen);

        // Calculate the ICMP header checksum
        icmphdr.icmp_cksum = calculate_checksum((unsigned short *) (packet), ICMP_HDR_LEN + datalen);
        memcpy((packet), &icmphdr, ICMP_HDR_LEN);


        //===================
        // sending
        //===================
        gettimeofday(&start, 0);
        // Send the packet using sendto() for sending datagrams.
        size_t bytes_sent = sendto(sock, packet, ICMP_HDR_LEN + datalen, 0, (struct sockaddr *) &dest_in,
                                   sizeof(dest_in));
        if (bytes_sent == -1) {
            fprintf(stderr, "[-] ping: icmp sendto() failed with error: %d\n", errno);
            return -1;
        }
        if (icmphdr.icmp_seq == 0) {
            printf("PING %s: %zu data bytes\n", dest_ip, bytes_sent);
        }


        //===================
        // receiving
        //===================
        bzero(packet, IP_MAXPACKET);
        socklen_t len = sizeof(dest_in);


        size_t bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *) &dest_in, &len);
        if (bytes_received <= 0) return 0;
        gettimeofday(&end, 0);


        //===================
        // print
        //===================
        // Check the ICMP header
        struct icmp *reply_icmphdr = (struct icmp *) (packet + sizeof(struct iphdr));

        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;

        if (strcmp(dest_ip, inet_ntoa(dest_in.sin_addr)) == 0) {
            send(sock_watchdog, "pet", 3, 0);
            printf("%zd bytes form %s: icmp_seq=%hu time=%f ms\n", bytes_received, inet_ntoa(dest_in.sin_addr),
                   reply_icmphdr->icmp_seq, milliseconds);
        } else {
            printf("Received from: %s but not from destination %s \n", inet_ntoa(dest_in.sin_addr), dest_ip);
        }
        sleep(1);
    }
}

int main(int argc, char *argv[]) {
    //===================
    // get ip from user
    //===================
    char *dest_ip = DEFAULT_DESTINATION_IP;
    if (argc > 1) {
        dest_ip = argv[1];
    }
    if (check_invalid_address(dest_ip)) {
        printf("[-] Invalid ip address: %s\n", dest_ip);
        return -1;
    }


    // create tcp connection with watchdog
    // and exec ./watchdog
    int sock_watchdog = execute_and_connectTo_watch_dog();
    if (sock_watchdog == -1) return -1;

    // Create raw socket for IP-RAW (make IP-header by yourself)
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
        printf("[-] ping: create raw-sock failed with error: %d, To create a raw socket, the process needs to be run by Admin/root user.\n",
               errno);
        return -1;
    }

    //fork so the recv of ping cant block the chat with watchdog
    int pid = fork();
    if (pid == 0) {
        chat_with_watchdog(sock_watchdog, dest_ip);
    } else {
        send_pings(sock, sock_watchdog, dest_ip);
    }
    // Close the raw socket descriptor.
    // end program
    close(sock_watchdog);
    close(sock);
    kill(pid, SIGTERM);
    return 0;
}
