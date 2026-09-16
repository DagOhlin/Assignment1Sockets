#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
//#define DEBUG
#define MAXDATASIZE 100


// Included to get the support library
#include <calcLib.h>

struct ParsedArgs {
    std::string protocol;
    std::string host;
    int port;
    std::string path;
};

ParsedArgs parse_url(const char *input) {
    if (!input || strstr(input, "///") != NULL) {
        fprintf(stderr, "ERROR: Invalid URL format\n");
        exit(EXIT_FAILURE);
    }

    char *proto_end = strstr((char *)input, "://");
    if (!proto_end) {
        fprintf(stderr, "ERROR: Missing '://'\n");
        exit(EXIT_FAILURE);
    }

    char *host_start = proto_end + 3;
    char *port_start = strchr(host_start, ':');
    char *path_start = strchr(host_start, '/');

    if (!port_start || !path_start || port_start >= path_start) {
        fprintf(stderr, "ERROR: Invalid host/port/path structure\n");
        exit(EXIT_FAILURE);
    }

    std::string protocol(input, proto_end - input);
    std::string host(host_start, port_start - host_start);
    std::string port_str(port_start + 1, path_start - (port_start + 1));
    std::string path(path_start + 1);

    int port = atoi(port_str.c_str());
    if (port < 1 || port > 65535) {
        fprintf(stderr, "ERROR: Port out of range\n");
        exit(EXIT_FAILURE);
    }

    return {protocol, host, port, path};
}

int setup_tcp(const std::string &host, int port) {
    struct addrinfo hints{}, *res, *p;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
        fprintf(stderr, "ERROR: RESOLVE ISSUE\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = -1;
    for (p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // Connection established
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd == -1) {
        fprintf(stderr, "ERROR: CANT CONNECT TO %s\n", host.c_str());
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

int main(int argc, char *argv[]){
  
  
  
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/path.\n", argv[0]);
    exit(EXIT_FAILURE);
  }


    ParsedArgs args = parse_url(argv[1]);
  
    printf("Host %s, and port %d.\n", args.host.c_str(), args.port);

    char buf [MAXDATASIZE];

    int sockfd = setup_tcp(args.host, args.port);

    int numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0);
    if (numbytes == -1) {
        perror("recv");
        close(sockfd);
        return 1;
    }

    buf[numbytes] = '\0'; 
#ifdef DEBUG 
    printf("Server sent:\n%s", buf);
#endif
     
    const char *msg = "TEXT TCP 1.1 OK\n";
    ssize_t bytes_sent = send(sockfd, msg, strlen(msg), 0);

    if (bytes_sent == -1) {
        perror("send");
    }

    numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0);
    if (numbytes == -1) {
        perror("recv");
        close(sockfd);
        return 1;
    }

    buf[numbytes] = '\0';
#ifdef DEBUG 
    printf("Server sent:\n%s", buf);
#endif
    memset(&buf, 0, sizeof buf);

}
