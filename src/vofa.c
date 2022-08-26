// Ref: https://gist.github.com/saxbophone/f770e86ceff9d488396c0c32d47b757e

#include "vofa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef WITH_GNU_COMPILER
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT "1346"

#define BUF_SIZE 500
static char buf[BUF_SIZE];

int sfd;
struct sockaddr_storage peer_addr;
socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
#endif

bool vofa_init(void) {
#ifdef WITH_GNU_COMPILER
    // binding
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // Datagram socket
    hints.ai_flags = AI_PASSIVE; // For wildcard IP address
    hints.ai_protocol = 0; // Any protocol
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    struct addrinfo *result;
    int s = getaddrinfo(NULL, PORT, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return true;
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully bind(2).
       If socket(2) (or bind(2)) fails, we (close the socket
       and) try the next address. */

    struct addrinfo *rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) break; // Success
        close(sfd);
    }

    if (rp == NULL) { // No address succeeded
        fprintf(stderr, "Could not bind\n");
        return true;
    }

    freeaddrinfo(result); // No longer needed
    
    printf("Waiting for VOFA+...\nSend any data from VOFA+ to establish the connection\n");

    // wait for client
    while (true) {
        ssize_t nread = recvfrom(sfd, buf, BUF_SIZE, 
            0, (struct sockaddr *)&peer_addr, &peer_addr_len);
        if (nread != -1) {
            // char host[NI_MAXHOST], service[NI_MAXSERV];
            // int s = getnameinfo((struct sockaddr *)&peer_addr, peer_addr_len, 
            //     host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
            // if (s == 0) {
            //     printf("Received %zd bytes from %s:%s\n", nread, host, service);
            // } else {
            //     fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
            // }
            printf("VOFA+ connected\n");
            return false;
        }
    }
#else
    return true;
#endif
}

bool vofa_send(char *buf, int buf_len) {
#ifdef WITH_GNU_COMPILER
    if (sendto(sfd, buf, buf_len, 
        0, (struct sockaddr *)&peer_addr, peer_addr_len
    ) != buf_len) {
        fprintf(stderr, "Error sending response\n");
        return true;
    }
    return false;
#else
    return true;
#endif
}