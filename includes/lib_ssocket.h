/* This library header defines common api for ssocket.dll */ 

#ifndef ms_lib_ssocket_h
#define ms_lib_ssocket_h 
#include <openssl/bio.h>
#include <openssl/ssl.h>

/* Define a new secure socket struct */

typedef struct {
    BIO* ssl_bio;
    SSL_CTX* ssl_ctx;
} SSOCKET;

#endif 
