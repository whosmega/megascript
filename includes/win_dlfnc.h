#ifndef ms_win_dlfcn_h
#ifdef _WIN32
#define ms_win_dlfcn_h 
#include <libloaderapi.h>

typedef void* HINSTANCE;            /* DLL handle type */ 

/* We define an inline function for handling dlopens, the second parameter is unused 
 * and is only present for compatibility purposes */ 
static inline HINSTANCE dlopen(const char* file, int _) {
    return LoadLibrary(TEXT(file));
}

/* Another inline function for indexing symbols from the handle */ 
static inline void* dlsym(HINSTANCE handle, const char* symbol) {
    return GetProcAddress(handle, symbol);
}

/* For closing the dll through the handle*/
static inline int dlclose(HINSTANCE handle) {
    return FreeLibrary(handle);
}

#endif 
#endif
