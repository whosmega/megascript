#include <sys/socket.h>         /* socket, connect */ 
#include <netinet/in.h>         /* sockaddr_in, sockaddr */ 
#include <netdb.h>              /* hostent, gethostbyname */ 

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../includes/msapi.h"
#include "../includes/memory.h"
#include "../includes/object.h"

typedef int SOCKET;

int writeSocket(SOCKET* socket, void* bytes, int length) {
    return 0;
}

int readSocket(SOCKET socket, void* dest, int length) {
    return 0;    
}

SOCKET _newSocket(VM* vm, const char* host, int port) {
    struct hostent* server;                 /* Server Struct */ 
    struct sockaddr_in server_addr;         /* Details about the server & connection */ 
    
    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        msapi_runtimeError(vm, "Error Opening Socket");
        return -1;
    }

    server = gethostbyname(host);
    if (server == NULL) {
        msapi_runtimeError(vm, "Unknown Host : '%s'", host);
        return -2;
    }
    
    /* Set fields of our server info struct */ 
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    int result = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (result < 0) {
        msapi_runtimeError(vm, "Could not connect to host : '%s'", host);
        return -3;
    }
    return sockfd;
} 

bool newSocket(VM* vm, int argCount, bool shouldReturn) {
    if (argCount < 2) {
        msapi_runtimeError(vm, "Too less arguments, expected 2, got %d", argCount);
        return false;
    }

    Value hostName = msapi_getArg(vm, 1, argCount);
    Value port = msapi_getArg(vm, 2, argCount);

    if (!CHECK_STRING(hostName)) {
        msapi_runtimeError(vm, "Expected host name to be a string");
        return false;
    }

    if (!CHECK_NUMBER(port)) {
        msapi_runtimeError(vm, "Expected port to be a number");
        return false;
    }
    
    msapi_popn(vm, argCount + 1);
    SOCKET sockfd = _newSocket(vm, AS_NATIVE_STRING(hostName), AS_NUMBER(port));

    if (shouldReturn) {
        ObjWebSocket* socket = allocateWebSocket(vm, sockfd);
        msapi_push(vm, OBJ(socket));
    }

    return true; 
}

bool closeSocket(VM* vm, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Too less arguments, expected 1, got 0");
        return false;
    }

    Value sockfd = msapi_getArg(vm, 1, argCount);

    if (!CHECK_WEB_SOCKET(sockfd)) {
        msapi_runtimeError(vm, "Expected sockfd to be a web socket");
        return false;
    }
    msapi_popn(vm, argCount + 1);
    ObjWebSocket* socket = AS_WEB_SOCKET(sockfd);

    if (socket->closed) {
        printf("Warning : Socket already closed"); 
    } else {
        close(socket->sockfd);
        socket->closed = true;
    }

    if (shouldReturn) {
        msapi_push(vm, NIL());
    }
    return true;
}
