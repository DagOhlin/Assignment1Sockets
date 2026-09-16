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
#define DEBUG
#define MAXDATASIZE 100


// Included to get the support library
#include <calcLib.h>

bool parseAndCalculate(const std::string &assignment, int &result) {
    char op[16];
    int val1, val2;
    if (sscanf(assignment.c_str(), "%15s %d %d", op, &val1, &val2) != 3) {
        return false;
    }
    std::string operation(op);
    if (operation == "add") result = val1 + val2;
    else if (operation == "sub") result = val1 - val2;
    else if (operation == "mul") result = val1 * val2;
    else if (operation == "div") {
        if (val2 == 0) return false;
        result = val1 / val2;
    } else return false;
    return true;
}

std::string toUpperCase(std::string str) {
    for (char &c : str) {
        c = std::toupper(static_cast<unsigned char>(c));
    }
    return str;
}

bool doesServerSuport(const std::string &advertised, const std::string &wanted) {
    return advertised.find(wanted) != std::string::npos;
}

void exitError(const std::string &msg, int sockfd = -1) {
    if (sockfd != -1) {
        close(sockfd);
    }
    fprintf(stderr, "ERROR: %s\n", msg.c_str());
    exit(EXIT_FAILURE);
}

int reciveFunc(int sockfd, char *buf, size_t maxLenght) {
    int numbytes = recv(sockfd, buf, maxLenght - 1, 0);
    if (numbytes == 0) {
        exitError("Server disconnected:(", sockfd);
    }
    if (numbytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            exitError("ERROR: MESSAGE LOST (TIMEOUT)", sockfd);
        }
        exitError("Receive failed", sockfd);
    }
    buf[numbytes] = '\0';
    return numbytes;
}

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

int setupTcp(const std::string &host, int port) {
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

    int sockfd = setupTcp(args.host, args.port);

    //timer for recive, beej used poll instead, could have advantages but this seams cleaner
    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    reciveFunc(sockfd, buf, MAXDATASIZE);


    std::string wantedProtocol = toUpperCase(args.path) + " " + toUpperCase(args.protocol) + " 1.1";

    if(doesServerSuport(buf, wantedProtocol)){

    }
    else{
        exitError("ERROR: MISSMATCH PROTOCOL\n", sockfd);
    }

    #ifdef DEBUG 
    printf("Server sent:\n%s", buf);
    #endif
     
    const char *msg = "TEXT TCP 1.1 OK\n";
    ssize_t bytes_sent = send(sockfd, msg, strlen(msg), 0);

    if (bytes_sent == -1) {
        perror("send");
    }
    memset(&buf, 0, sizeof(buf));
    reciveFunc(sockfd, buf, MAXDATASIZE);

    std::string assignment(buf);

    #ifdef DEBUG
    printf("Server sent:\n%s", buf);
    #endif

    int res;
    if (!parseAndCalculate(assignment, res)) {
        exitError("Could not parse assignment", sockfd);
    }

    
    printf("ASSIGNMENT: %s\n", assignment.c_str());

    #ifdef DEBUG
    printf("gott %d\n", res);
    #endif

    std::string response = std::to_string(res) + "\n";
    bytes_sent = send(sockfd, response.c_str(), response.length(), 0);
    if (bytes_sent == -1) {
        perror("send");
    }

    memset(&buf, 0, sizeof(buf));
    
    reciveFunc(sockfd, buf, MAXDATASIZE);

    std::string serverReply(buf);
    if (!serverReply.empty() && serverReply.back() == '\n') {
        serverReply.pop_back();
    }
    printf("%s (myresult=%d)\n", serverReply.c_str(), res);

    close(sockfd);
    return EXIT_SUCCESS;

}
