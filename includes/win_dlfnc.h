#ifndef ms_win_dlfcn_h
#ifdef _WIN32
#define ms_win_dlfcn_h 
#include <libloaderapi.h>

/* We define an inline function for handling dlopens, the second parameter is unused 
 * and is only present for compatibility purposes */ 
static inline HINSTANCE dlopen(const char* file, int _) {
    return (void*)LoadLibraryA(TEXT(file));
}

/* Another inline function for indexing symbols from the handle */ 
static inline void* dlsym(void* handle, const char* symbol) {
    return GetProcAddress((HINSTANCE)handle, symbol);
}

static inline char* dlerror() {
    return "Detailed DLL errors aren't implemented for windows";
}

/* For closing the dll through the handle*/
static inline int dlclose(void* handle) {
    return FreeLibrary((HINSTANCE)handle);
}

#endif 
#endif
