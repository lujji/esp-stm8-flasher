#ifndef SPIFFS_CHECK_H
#define SPIFFS_CHECK_H

#include "telnet_printf.h"
#include <spiffs.h>

/* just print the error, no return */
#define SPIFFS_CHECK_INF(x) \
    if ((x) < 0) { telnet_printf("errno:%i\n", SPIFFS_errno(&fs));}

/* check result and return on error */
#define SPIFFS_CHECK(x) \
    if ((x) < 0) { telnet_printf("errno:%i\n", SPIFFS_errno(&fs)); return -1; }

/* check result and close file handle on error */
#define SPIFFS_CHECK_SAFE(x, fd) \
    if ((x) < 0) { telnet_printf("errno:%i\n", SPIFFS_errno(&fs)); \
                   SPIFFS_close(&fs, fd); return -1; }

#endif /* SPIFFS_CHECK_H */

