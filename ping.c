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
#include <unistd.h>

// IPv4 header len without options
#define IP4_HDR_LEN 20

// ICMP header len for echo req
#define ICMP_HDR_LEN 8
#define BUFFER_SIZE  64

// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);

int check_invalid_address(char *address);

int send_pings(int sock, char *dest_ip);

// 1. Change SOURCE_IP and DEFAULT_DESTINATION_IP to the relevant
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

    send_pings(sock, dest_ip);

    // Close the raw socket descriptor.
    close(sock);


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

int check_invalid_address(char *address) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, address, &(sa.sin_addr));
    return result == 0;
}

int send_pings(int sock, char *dest_ip) {
    struct timeval start, end;
    struct icmp icmphdr; // ICMP-header
    int seq_counter =0;
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
            fprintf(stderr, "sendto() failed with error: %d\n", errno);
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
        if (bytes_received <= 0)return -1;
        gettimeofday(&end, 0);


        //===================
        // print
        //===================
        // Check the ICMP header
        struct icmp *reply_icmphdr = (struct icmp *) (packet + sizeof(struct iphdr));

        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;


        printf("%zd bytes form %s: icmp_seq=%hu time=%f ms\n", bytes_received, inet_ntoa(dest_in.sin_addr),
               reply_icmphdr->icmp_seq, milliseconds);
        sleep(1);
    }
}

