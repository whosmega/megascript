/* This library defines the basic functions for simple HTTP/WS connections 
 * to servers, this library does not work for any HTTPS/WSS connections and 
 * the ssocket library should be used instead. Both have a similar interface 
 * which is wrapped up by higher level code to allow REST API Requests */


#include <sys/socket.h>         /* socket, connect */ 
#include <netinet/in.h>         /* sockaddr_in, sockaddr */ 
#include <netdb.h>              /* hostent, gethostbyname */ 

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "../includes/msapi.h"
#include "../includes/memory.h"
#include "../includes/object.h"

typedef int SOCKET;

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

int _readSocket(VM* vm, SOCKET sockfd, char** string) {
    int max = 1300;
    char response[max + 1];
    int currentRead = 0;


    currentRead = read(sockfd, response, max);
        
    if (currentRead == -1) {
        /* An error occured */ 
        return -1;
    }

    if (currentRead == 0) {
        return 0;
    }
    
    /* We read something */
    char* chars = (char*)reallocate(vm, NULL, 0, sizeof(char) * currentRead + 1);
    memcpy(chars, response, currentRead);
    chars[currentRead] = '\0';

    *string = chars;
    return currentRead;
}

bool _writeSocket(SOCKET sockfd, char* chars, int length) {
    int bytesWritten = 0;
    int currentWritten = 0;

    do {
        currentWritten = write(sockfd, chars + bytesWritten, length - bytesWritten);
        
        if (currentWritten == -1) {
            return false;
        }

        if (currentWritten == 0) {
            break;
        }
        bytesWritten += currentWritten;
    } while (bytesWritten < length);

    return true;
}

bool newSocket(VM* vm, Obj* self, int argCount, bool shouldReturn) {
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
    
    SOCKET sockfd = _newSocket(vm, AS_NATIVE_STRING(hostName), AS_NUMBER(port));
    
    if (sockfd < 0) {
        return false;
    }

    if (shouldReturn) {
        ObjSocket* socket = allocateSocket(vm, sockfd);
        msapi_popn(vm, argCount + 1);
        msapi_push(vm, OBJ(socket));
    } else {
        msapi_popn(vm, argCount + 1);
    }

    return true; 
}

bool closeSocket(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Too less arguments, expected 1, got 0");
        return false;
    }

    Value sockfd = msapi_getArg(vm, 1, argCount);

    if (!CHECK_SOCKET(sockfd)) {
        msapi_runtimeError(vm, "Expected sockfd to be a web socket");
        return false;
    }
    msapi_popn(vm, argCount + 1);
    ObjSocket* socket = AS_SOCKET(sockfd);

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

bool readSocket(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Too less arguments, expected 1, got 0");
        return false;
    }

    Value sockfd = msapi_getArg(vm, 1, argCount);

    if (!CHECK_SOCKET(sockfd)) {
        msapi_runtimeError(vm, "Expected sockfd to be a web socket");
        return false;
    }

    ObjSocket* socket = AS_SOCKET(sockfd);
    if (socket->closed) {
        msapi_runtimeError(vm, "Socket has already been closed");
        return false;
    }
    
    char* data = NULL;
    int length = _readSocket(vm, socket->sockfd, &data);
    
    if (length == 0) {
        /* We did not read anything */ 
        msapi_popn(vm, argCount + 1);

        if (shouldReturn) {
            msapi_push(vm, NIL());
        }

        return true;
    } else if (length == -1) {
        msapi_runtimeError(vm, "An unknown error occured while reading from socket");
        return false;
    }

    ObjString* string = allocateString(vm, data, length); 
    msapi_popn(vm, argCount + 1);
    
    if (shouldReturn) {
        msapi_push(vm, OBJ(string));
    }

    return true;
}

bool writeSocket(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 2) {
        msapi_runtimeError(vm, "Too less arguments, expected 2, got %d", argCount);
        return false;
    }

    Value sockfd = msapi_getArg(vm, 1, argCount);
    Value string = msapi_getArg(vm, 2, argCount);
    if (!CHECK_SOCKET(sockfd)) {
        msapi_runtimeError(vm, "Expected sockfd to be a web socket");
        return false;
    }

    ObjSocket* socket = AS_SOCKET(sockfd);
    if (socket->closed) {
        msapi_runtimeError(vm, "Socket has already been closed");
        return false;
    }

    if (!CHECK_STRING(string)) {
        msapi_runtimeError(vm, "Expected writing value to be a string");
        return false;
    }
    
    ObjString* stringObj = AS_STRING(string);
    bool success = _writeSocket(socket->sockfd, stringObj->allocated, stringObj->length);

    if (!success) {
        msapi_runtimeError(vm, "An error occured while writing to socket");
        return false;
    }

    msapi_popn(vm, argCount + 1);
    if (shouldReturn) {
        msapi_push(vm, NIL());
    }
    
    return true;
}
