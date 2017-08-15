#ifndef TELNET_PRINTF_H
#define TELNET_PRINTF_H

#define TELNET_PRINTF           -1
#define TELNET_PRINTF_PORT      7

void disable_stdout();

void telnet_printf_task(void *arg);

int _telnet_printf(const char *fmt, ...);

#if TELNET_PRINTF == 1
#define telnet_printf(...) { _telnet_printf(__VA_ARGS__); }
#elif TELNET_PRINTF == 0
#define telnet_printf(...) { printf(__VA_ARGS__); }
#else
#define telnet_printf(...) { }
#endif

#endif /* TELNET_PRINTF_H */
