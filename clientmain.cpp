#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>

#include "protocol.h"
// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
//#define DEBUG
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

int sendFunc(int sockfd, const void *buf, size_t len) {
    const char *ptr = static_cast<const char *>(buf);
    size_t totalSent = 0;

    while (totalSent < len) {
        ssize_t numbytes = send(sockfd, ptr + totalSent, len - totalSent, 0);

        if (numbytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                exitError("ERROR: MESSAGE LOST (TIMEOUT)", sockfd);
            }
            exitError("Send failed", sockfd);
        }
        if (numbytes == 0) {
            exitError("Server disconnected:(", sockfd);
        }

        totalSent += numbytes;
    }

    return static_cast<int>(totalSent);
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
    #ifdef DEBUG 
    printf("Server sent:\n%s", buf);
    #endif
    return numbytes;
}

int reciveStruct(int sockfd, void *buf, size_t expectedLen) {
    ssize_t numbytes = recv(sockfd, buf, expectedLen, 0);
    if (numbytes == 0) {
        exitError("Server disconnected:(", sockfd);
    }
    if (numbytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            exitError("MESSAGE LOST (TIMEOUT)", sockfd);
        }
        exitError("Receive failed", sockfd);
    }
    if ((size_t)numbytes != expectedLen) {
        exitError("WRONG SIZE OR INCORRECT PROTOCOL", sockfd);
    }
    return numbytes;
}

struct ParsedArgs {
    std::string protocol;
    std::string host;
    int port;
    std::string api;
};

ParsedArgs parseUrl(const char *input) {
    if (!input || strstr(input, "///") != NULL) {
        fprintf(stderr, "ERROR: Invalid URL format\n");
        exit(EXIT_FAILURE);
    }

    char *protoEnd = strstr((char *)input, "://");
    if (!protoEnd) {
        fprintf(stderr, "ERROR: Missing '://'\n");
        exit(EXIT_FAILURE);
    }

    char *hostStart = protoEnd + 3;
    char *portStart = strchr(hostStart, ':');
    char *apiStart = strchr(hostStart, '/');

    if (!portStart || !apiStart || portStart >= apiStart) {
        fprintf(stderr, "ERROR: Invalid host/port/api structure\n");
        exit(EXIT_FAILURE);
    }

    std::string protocol(input, protoEnd - input);
    std::string host(hostStart, portStart - hostStart);
    std::string portStr(portStart + 1, apiStart - (portStart + 1));
    std::string api(apiStart + 1);

    int port = atoi(portStr.c_str());
    if (port < 1 || port > 65535) {
        fprintf(stderr, "ERROR: Port out of range\n");
        exit(EXIT_FAILURE);
    }

    return {protocol, host, port, api};
}

int setupTcp(const std::string &host, int port, bool exitOnFailure = true) {
    struct addrinfo hints{}, *res, *p;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        if (!exitOnFailure) return -1;
        fprintf(stderr, "ERROR: RESOLVE ISSUE\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = -1;
    for (p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd == -1) {
        if (!exitOnFailure) return -1;
        fprintf(stderr, "ERROR: CANT CONNECT TO %s\n", host.c_str());
        exit(EXIT_FAILURE);
    }
    return sockfd;
}


int connectTcp(const std::string &host, int port, const std::string &apiUpper,
               bool exitOnFailure = true) {
    int sockfd = setupTcp(host, port, exitOnFailure);
    if (sockfd == -1) return -1;

    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buf[MAXDATASIZE];
    memset(buf, 0, sizeof(buf));
    int n = recv(sockfd, buf, MAXDATASIZE - 1, 0);
    if (n <= 0) {
        close(sockfd);
        if (!exitOnFailure) return -1;
        exitError("MESSAGE LOST (TIMEOUT)");
    }
    buf[n] = '\0';

    
    std::string wantedProtocol = apiUpper + " TCP 1.1";
    if (!doesServerSuport(buf, wantedProtocol)) {
        close(sockfd);
        if (!exitOnFailure) return -1;
        exitError("MISSMATCH PROTOCOL");
    }

    std::string acceptMessage = wantedProtocol + " OK\n";
    sendFunc(sockfd, acceptMessage.c_str(), acceptMessage.length());
    return sockfd;
}



void handleTextAssignment(int sockfd, const std::string &assignment) {
    std::string trimmedAssignment = assignment;
    int res;
    if (!parseAndCalculate(assignment, res)) {
        exitError("Could not parse assignment", sockfd);
    }

    //added this to fix double newline, not sure if codegrade cares
    if (!trimmedAssignment.empty() && trimmedAssignment.back() == '\n') {
        trimmedAssignment.pop_back();
    }
    printf("ASSIGNMENT: %s\n", trimmedAssignment.c_str());

    #ifdef DEBUG
    printf("gott %d\n", res);
    #endif

    std::string response = std::to_string(res) + "\n";
    sendFunc(sockfd, response.c_str(), response.length());

    char buf[MAXDATASIZE];
    memset(&buf, 0, sizeof(buf));
    reciveFunc(sockfd, buf, MAXDATASIZE);

    std::string serverReply(buf);
    if (!serverReply.empty() && serverReply.back() == '\n') {
        serverReply.pop_back();
    }
    printf("%s (myresult=%d)\n", serverReply.c_str(), res);

    close(sockfd);
}

void handleTcpText(int sockfd) {
    char buf[MAXDATASIZE];
    memset(&buf, 0, sizeof(buf));
    reciveFunc(sockfd, buf, MAXDATASIZE);
    handleTextAssignment(sockfd, std::string(buf));
}

void handleUdpText(int sockfd) {
    const char *hello = "TEXT UDP 1.1\n";
    sendFunc(sockfd, hello, strlen(hello));

    char buf[MAXDATASIZE];
    memset(&buf, 0, sizeof(buf));
    reciveFunc(sockfd, buf, MAXDATASIZE);
    handleTextAssignment(sockfd, std::string(buf));
}

int setupUdp(const std::string &host, int port, bool exitOnFailure = true) {
    struct addrinfo hints{}, *res, *p;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        if (!exitOnFailure) return -1;
        fprintf(stderr, "ERROR: RESOLVE ISSUE\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = -1;
    for (p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        //still usng connect for udp so i dont have to use sentTo etc
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd == -1) {
        if (!exitOnFailure) return -1;
        fprintf(stderr, "ERROR: CANT CONNECT TO %s\n", host.c_str());
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void handleBinaryAssignment(int sockfd) {
    calcProtocol msg;
    reciveStruct(sockfd, &msg, sizeof(msg));


    uint16_t type = ntohs(msg.type);
    uint32_t arith = ntohl(msg.arith);
    int32_t val1 = ntohl(msg.inValue1);
    int32_t val2 = ntohl(msg.inValue2);

   
    const char *opName;
    int32_t result;
    switch (arith) {
        case 1: opName = "add"; result = val1 + val2; break;
        case 2: opName = "sub"; result = val1 - val2; break;
        case 3: opName = "mul"; result = val1 * val2; break;
        case 4:
            opName = "div";
            if (val2 == 0) exitError("Division by zero", sockfd);
            result = val1 / val2;
            break;
        default:
            exitError("Unknown arith code", sockfd);
    }

    printf("ASSIGNMENT: %s %d %d\n", opName, val1, val2);

    #ifdef DEBUG
    printf("Calculated %d\n", result);
    #endif

    msg.type = htons(2);
    msg.inResult = htonl(result);
    sendFunc(sockfd, &msg, sizeof(msg));

    calcMessage reply;
    reciveStruct(sockfd, &reply, sizeof(reply));

    uint32_t replyMessage = ntohl(reply.message);
    if (replyMessage == 1) {
        printf("OK (myresult=%d)\n", result);
    } else {
        printf("ERROR (myresult=%d)\n", result);
    }

    close(sockfd);
}

void handleTcpBinary(int sockfd) {
    handleBinaryAssignment(sockfd);
}

void handleUdpBinary(int sockfd) {
    calcMessage hello{};
    hello.type = htons(22);
    hello.message = htonl(0);
    hello.protocol = htons(17); // UDP
    hello.major_version = htons(1);
    hello.minor_version = htons(1);
    sendFunc(sockfd, &hello, sizeof(hello));

    handleBinaryAssignment(sockfd);
}

int main(int argc, char *argv[]){
  
  
  
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/api.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  ParsedArgs args = parseUrl(argv[1]);
  
    printf("Host %s, and port %d.\n", args.host.c_str(), args.port);

    std::string protoUpper = toUpperCase(args.protocol);
    std::string apiUpper = toUpperCase(args.api);
    if (apiUpper != "TEXT" && apiUpper != "BINARY") exitError("Unknown api");

    int sockfd;
    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};

    if (protoUpper == "TCP") {
        sockfd = connectTcp(args.host, args.port, apiUpper);
        if (apiUpper == "TEXT") handleTcpText(sockfd); else handleTcpBinary(sockfd);

    } else if (protoUpper == "UDP") {
        sockfd = setupUdp(args.host, args.port);
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        if (apiUpper == "TEXT") handleUdpText(sockfd); else handleUdpBinary(sockfd);

    } else if (protoUpper == "ANY") {
        sockfd = connectTcp(args.host, args.port, apiUpper, false);
        if (sockfd != -1) {
            printf("Reached server using TCP.\n");
            if (apiUpper == "TEXT") handleTcpText(sockfd); else handleTcpBinary(sockfd);
        } else {
            sockfd = setupUdp(args.host, args.port, false);
            if (sockfd == -1) exitError("CANT CONNECT TO " + args.host);
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            printf("Reached server using UDP.\n");
            if (apiUpper == "TEXT") handleUdpText(sockfd); else handleUdpBinary(sockfd);
        }

    } else {
        exitError("Unknown protocol");
    }

    return EXIT_SUCCESS;

}
