#ifndef ms_win_unistd_h 
#define ms_win_unistd_h 

/* My port for some functions of UNIX standard library for windows */
#include <io.h>
#include <stdlib.h>

#define R_OK 4              /* Read Access */ 
#define W_OK 2              /* Write Access */ 
#define F_OK 0              /* File Existence Access */ 

#define access _access 

#endif
