#include "../includes/msapi.h"
#include "../includes/lib_ssocket.h"
#include "../includes/memory.h"
#include "../includes/object.h"
#include <openssl/bioerr.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509_vfy.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

/* Read Socket might need to be called in a loop to get the full response */

int _readSocket(VM* vm, SSOCKET* ssocket, char** bufferPtr) {
    int max = 1300;
    char response[max + 1];
    int result = 0;
    result = BIO_read(ssocket->ssl_bio, response, max);
    if (result == -1) {
        return -1;
    } else if (result == 0) {
        return 0;
    }
    
    char* string = (char*)reallocate(vm, NULL, 0, sizeof(char) * result + 1);
    string[result] = '\0';
    memcpy(string, response, result);
    *bufferPtr = string;
    return result; 
}

int _writeSocket(SSOCKET* ssocket, char* chars, int length) {
    int result = 0, totalWrite = 0;

    for (;;) {
        if (totalWrite == length) return result;
        result = BIO_write(ssocket->ssl_bio, chars, length);
        
        if (result <= 0) {
            return -1;
        }

        totalWrite += result;
    }
}

SSOCKET* _newSocket(VM* vm, char* host, int port) {
    /* Basic initalization for openssl */ 
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
    
    /* We convert the port to a string so we can concatenate it with 
     * our hostname later on */
    char portStr[20];
    if (snprintf(portStr, 20, "%d", port) < 0) {
        msapi_runtimeError(vm, "Invalid Port");
        return NULL;
    }

    /* We write our hostname and port in the correct format 'hostname:port' */ 
    char fullHost[strlen(host) + 1 + strlen(portStr)];
    strcpy(fullHost, host);
    strcat(fullHost, ":");
    strcat(fullHost, portStr);

    /* We allocate a new client side SSL context to use for our connection */ 
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    SSL* ssl = NULL;
    /* We make a new BIO object which also implicitly allocates a SSL struct */ 
    BIO* bio = BIO_new_ssl_connect(ctx);

    if (bio == NULL) {
        msapi_runtimeError(vm, "Error creating OpenSSL BIO");
        return NULL;
    }
    
    /* We setup the hostname in the BIO */ 
    BIO_set_conn_hostname(bio, fullHost);
    /* We retrieve the internal SSL struct so we can modify it */ 
    BIO_get_ssl(bio, &ssl);
    /* We enable the auto retry flag so if the server ever requests a handshake,
     * openssl handles it for us */ 
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY); 

    /* We load the valid certificates folder to tell openssl which directory 
     * to look in when checking certificates */ 

    char* name = findFile(vm, "certs", true); 
    if (!SSL_CTX_load_verify_locations(ctx, NULL, name)) {
        msapi_runtimeError(vm, "An Error Occured While Loading Certificates");
        return NULL;
    }
    reallocateArray(name, sizeof(*name), 0);
   
    /* We attempt to connect to the server */ 

    if (BIO_do_connect(bio) <= 0) {
        ERR_print_errors_fp(stdout);
        msapi_runtimeError(vm, "Connection with the server failed");
        return NULL;
    }
    
   
    /* We will need the BIO to perform read and write operations and 
     * we also need to free the context later on, so we wrap it in our 
     * own struct and return it */ 

    SSOCKET* ssocket = (SSOCKET*)reallocateArray(NULL, 0, sizeof(SSOCKET));
    ssocket->ssl_bio = bio; 
    ssocket->ssl_ctx = ctx;
    
    return ssocket;
}

void _closeSocket(SSOCKET* ssocket) {
    /* Close and free the sockets */ 

    BIO_free_all(ssocket->ssl_bio);
    SSL_CTX_free(ssocket->ssl_ctx);

    /* Free our struct */ 
    reallocateArray(ssocket, sizeof(SSOCKET), 0);
}


bool newSocket(VM* vm, Obj* self, int argCount, bool shouldReturn) {
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

    ObjSSocket* s = allocateSSocket(vm, ssocket);
    
    msapi_popn(vm, argCount + 1);
    
    if (shouldReturn) {
        msapi_push(vm, OBJ(s));
    }

    return true;
}

bool closeSocket(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Too less arguments, expected 1, got 0");
        return false;
    }

    Value socket = msapi_getArg(vm, 1, argCount);

    if (!CHECK_SSOCKET(socket)) {
        msapi_runtimeError(vm, "Expected a secure websocket object");
        return false;
    }

    ObjSSocket* ssocket = AS_SSOCKET(socket);

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

    return true;
}

bool readSocket(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 1) {
        msapi_runtimeError(vm, "Too less arguments, expected 1, got 0");
        return false;
    }

    Value socket = msapi_getArg(vm, 1, argCount);

    if (!CHECK_SSOCKET(socket)) {
        msapi_runtimeError(vm, "Expected a secure web socket object");
        return false;
    }

    ObjSSocket* ssocket = AS_SSOCKET(socket);
    
    if (ssocket->closed) {
        msapi_runtimeError(vm, "Attempt to read from a closed socket");
        return false;
    }
    
    char* chars = NULL;
    int length = _readSocket(vm, ssocket->ssocket, &chars);

    if (length == -1) {
        msapi_runtimeError(vm, "An error occured when trying to read from socket");
        return false;
    }
    
    if (chars == NULL) {
        msapi_popn(vm, argCount + 1);
        msapi_push(vm, NIL());
        return true;
    }
    ObjString* string = allocateString(vm, chars, length);
    reallocate(vm, chars, sizeof(char) * length + 1, 0);
    msapi_popn(vm, argCount + 1);
    msapi_push(vm, OBJ(string));
    return true;
}

bool writeSocket(VM* vm, Obj* self, int argCount, bool shouldReturn) {
    if (argCount < 2) {
        msapi_runtimeError(vm, "Too less arguments, expected 2, got %d", argCount);
        return false;
    }

    Value socket = msapi_getArg(vm, 1, argCount);
    Value str = msapi_getArg(vm, 2, argCount);

    if (!CHECK_SSOCKET(socket)) {
        msapi_runtimeError(vm, "Expected secure web socket object");
        return false;
    }

    if (!CHECK_STRING(str)) {
        msapi_runtimeError(vm, "Expected write data to be a string");
        return false;
    }

    ObjSSocket* ssocket = AS_SSOCKET(socket);
    ObjString* data = AS_STRING(str);
    
    if (ssocket->closed) {
        msapi_runtimeError(vm, "Attempt to write to a closed socket");
        return false;
    }

    int result = _writeSocket(ssocket->ssocket, data->allocated, data->length);

    if (result == -1) {
        msapi_runtimeError(vm, "An error occured while writing data to socket");
        return false;
    }
    
    msapi_popn(vm, argCount + 1);

    return true;
}
