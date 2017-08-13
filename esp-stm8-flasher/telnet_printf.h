#ifndef TELNET_PRINTF_H
#define TELNET_PRINTF_H

#include "config.h"

void disable_stdout();

void telnet_printf_task(void *arg);

#if TELNET_PRINTF
int telnet_printf(const char *fmt, ...);
#else
#define telnet_printf(...) { printf(__VA_ARGS__); }
#endif

#endif /* TELNET_PRINTF_H */
