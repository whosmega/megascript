#include "../includes/msapi.h"
#include "../includes/lib_ssocket.h"
#include "../includes/memory.h"
#include "../includes/object.h"
#include <openssl/bioerr.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <stdio.h>
#include <string.h>

SSOCKET* _newSocket(VM* vm, char* host, int port) {
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
    
    char portStr[20];
    if (snprintf(portStr, 20, "%d", port) < 0) {
        msapi_runtimeError(vm, "Invalid Port");
        return NULL;
    }
    char fullHost[strlen(host) + 1 + strlen(portStr)];
    strcpy(fullHost, host);
    strcat(fullHost, ":");
    strcat(fullHost, portStr);

    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    SSL* ssl;
    BIO* bio = BIO_new_connect(fullHost);

    if (bio == NULL) {
        msapi_runtimeError(vm, "Error creating OpenSSL BIO");
        return NULL;
    }

    if (BIO_do_connect(bio) <= 0) {
        msapi_runtimeError(vm, "Connection with the server failed");
        return NULL;
    }

    SSOCKET* ssocket = (SSOCKET*)reallocateArray(NULL, 0, sizeof(SSOCKET));
    ssocket->ssl_bio = bio; 
    ssocket->ssl_ctx = NULL;

    return ssocket;
}

void _closeSocket(SSOCKET* ssocket) {
    /* Close and free the sockets */ 

    BIO_free_all(ssocket->ssl_bio);
    // SSL_CTX_free(ssocket->ssl_ctx);

    /* Free our struct */ 
    reallocateArray(ssocket, sizeof(SSOCKET), 0);
}

bool newSocket(VM* vm, int argCount, bool shouldReturn) {
    if (argCount < 2) {
        msapi_runtimeError(vm, "Too less arguments, expected 2, got %d", argCount);
        return false;
    }

    Value host = msapi_getArg(vm, 1, argCount);
    Value port = msapi_getArg(vm, 2, argCount);

    if (!CHECK_STRING(host)) {
        msapi_runtimeError(vm, "Expected host to be a string");
        return false;
    }

    if (!CHECK_NUMBER(port)) {
        msapi_runtimeError(vm, "Expected port to be a number");
        return false;
    }

    SSOCKET* ssocket = _newSocket(vm, AS_NATIVE_STRING(host), AS_NUMBER(port));
    
    if (ssocket == NULL) {
        return false;
    }

    ObjWebSSocket* s = allocateWebSSocket(vm, ssocket);
    
    msapi_popn(vm, argCount + 1);
    
    if (shouldReturn) {
        msapi_push(vm, OBJ(s));
    }

    printf("Opened socket\n");

    return true;
}

bool closeSocket(VM* vm, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Too less arguments, expected 1, got 0");
        return false;
    }

    Value socket = msapi_getArg(vm, 1, argCount);

    if (!CHECK_WEB_SSOCKET(socket)) {
        msapi_runtimeError(vm, "Expected a secure websocket object");
        return false;
    }

    ObjWebSSocket* ssocket = AS_WEB_SSOCKET(socket);

    if (ssocket->closed) {
        printf("Warning : Socket already closed\n");
        return true;
    }

    _closeSocket(ssocket->ssocket);
    ssocket->ssocket = NULL;
    ssocket->closed = true;
    
    msapi_popn(vm, argCount + 1);

    if (shouldReturn) {
        msapi_push(vm, NIL());
    }
    
    printf("Closed socket\n");

    return true;
}
