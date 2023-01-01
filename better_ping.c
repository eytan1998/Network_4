// icmp.cpp
// Robert Iakobashvili for Ariel uni, license BSD/MIT/Apache
//
// Sending ICMP Echo Requests using Raw-sockets.
//

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>

// IPv4 header len without options
#define IP4_HDRLEN 20

// ICMP header len for echo req
#define ICMP_HDRLEN 8

// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);

int check_invalid_address(char *adress);

int send_ping(int sock, int, char *dest_ip);

// 1. Change SOURCE_IP and DESTINATION_IP to the relevant
//     for your computer
// 2. Compile it using MSVC compiler or g++
// 3. Run it from the account with administrative permissions,
//    since opening of a raw-socket requires elevated preveledges.
//
//    On Windows, right click the exe and select "Run as administrator"
//    On Linux, run it as a root or with sudo.
//
// 4. For debugging and development, run MS Visual Studio (MSVS) as admin by
//    right-clicking at the icon of MSVS and selecting from the right-click
//    menu "Run as administrator"
//
//  Note. You can place another IP-source address that does not belong to your
//  computer (IP-spoofing), i.e. just another IP from your subnet, and the ICMP
//  still be sent, but do not expect to see ICMP_ECHO_REPLY in most such cases
//  since anti-spoofing is wide-spread.

// i.e the gateway or ping to google.com for their ip-address
#define DEFAULT_DESTINATION_IP "8.8.8.8"

int main(int argc, char *argv[]) {
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        return -1;
    } else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(3000);

    // Binding newly created socket to given IP and verification
    unlink("Path/to/Socket");
    if ((bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        return -1;
    } else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        return -1;
    } else
        printf("Server listening..\n");
    len = sizeof(cli);

    /*char *args[2];
    // compiled watchdog.c by makefile
    args[0] = "./watchdog";
    args[1] = NULL;
    int status;
    int pid = fork();
    if (pid == 0)
    {
        printf("in child \n");
        execvp(args[0], args);
    }
*/
    // Accept the data packet from client and verification
    connfd = accept(sockfd, (struct sockaddr *) &cli, &len);
    if (connfd < 0) {
        printf("server accept failed...\n");
        return -1;
    } else
        printf("server accept the client...\n");
    close(sockfd);


    //****************************************************************************
    char *dest_ip = DEFAULT_DESTINATION_IP;
    if (argc > 1) {
        dest_ip = argv[1];
    }
    if (check_invalid_address(dest_ip)) {
        printf("[-] Invalid ip address: %s\n", dest_ip);
        return -1;
    }

    // Create raw socket for IP-RAW (make IP-header by yourself)
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, ", To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }

    send_ping(sock, connfd, dest_ip);

    // Close the raw socket descriptor.
    close(sock);
    close(sockfd);
    close(connfd);


    return 0;
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

int check_invalid_address(char *adress) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, adress, &(sa.sin_addr));
    return result == 0;
}

int send_ping(int sock, int connfd, char *dest_ip) {

    struct icmp icmphdr; // ICMP-header
    icmphdr.icmp_seq = -1;
    char msg[4] = {0};

    int pid = fork();
    if (pid == 0) {
        while (1) {
            strcpy(msg,"add\0");
            send(connfd, msg, 4, 0);
            bzero(msg, 4);
            recv(connfd, msg, 4, 0);
            if (strcmp(msg, "end") == 0) {
                printf("Server %s cannot be reached.\n", dest_ip);
                shutdown(sock, SHUT_RDWR);
                return -1;
            }
        }
        //watch dog
    } else {
        //ping

        while (1) {
            char data[IP_MAXPACKET] = "This is the ping.\n";

            unsigned int datalen = strlen(data) + 1;

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
            icmphdr.icmp_seq += 1;

            // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
            icmphdr.icmp_cksum = 0;

            // Combine the packet
            char packet[IP_MAXPACKET];

            // Next, ICMP header
            memcpy((packet), &icmphdr, ICMP_HDRLEN);

            // After ICMP header, add the ICMP data.
            memcpy(packet + ICMP_HDRLEN, data, datalen);

            // Calculate the ICMP header checksum
            icmphdr.icmp_cksum = calculate_checksum((unsigned short *) (packet), ICMP_HDRLEN + datalen);
            memcpy((packet), &icmphdr, ICMP_HDRLEN);

            struct sockaddr_in dest_in;
            memset(&dest_in, 0, sizeof(struct sockaddr_in));
            dest_in.sin_family = AF_INET;

            // The port is irrelant for Networking and therefore was zeroed.
            // dest_in.sin_addr.s_addr = iphdr.ip_dst.s_addr;
            dest_in.sin_addr.s_addr = inet_addr(dest_ip);
            // inet_pton(AF_INET, DESTINATION_IP, &(dest_in.sin_addr.s_addr));

            struct timeval start, end;
            gettimeofday(&start, 0);

            // Send the packet using sendto() for sending datagrams.
            size_t bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *) &dest_in,
                                       sizeof(dest_in));
            if (bytes_sent == -1) {
                fprintf(stderr, "sendto() failed with error: %d\n", errno);
                return -1;
            }
            bzero(msg, 4);
            strcpy(msg, "pet\0");
            send(connfd, msg, strlen(msg) + 1, 0);

            if (icmphdr.icmp_seq == 0) {
                printf("PING %s: %zu data bytes\n", dest_ip, bytes_sent);
            }

            // Get the ping response
            bzero(packet, IP_MAXPACKET);
            socklen_t len = sizeof(dest_in);
            ssize_t bytes_received = -1;

            bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *) &dest_in, &len);
            if (bytes_received <= 0)return -1;

            gettimeofday(&end, 0);

            char reply[IP_MAXPACKET];
            memcpy(reply, packet + ICMP_HDRLEN + IP4_HDRLEN, datalen);

            float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;

            printf("%zd bytes form %s: icmp_seq=%d time=%f ms\n", bytes_received, dest_ip, icmphdr.icmp_seq,
                   milliseconds);
            sleep(1);
        }
    }
}