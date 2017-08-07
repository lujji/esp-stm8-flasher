#include "telnet_printf.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <lwip/sys.h>
#include <lwip/api.h>

static struct netconn *printf_client = NULL;

void telnet_printf_task(void *arg) {
    LWIP_UNUSED_ARG(arg);
    struct netconn *nc = NULL;

    while (1) {
        if (!nc) {
            nc = netconn_new(NETCONN_TCP);
            if (!nc) {
                telnet_printf("Failed to allocate socket.\n");
                break;
            }
            netconn_bind(nc, IP_ADDR_ANY, TELNET_PRINTF_PORT);
            netconn_listen_with_backlog(nc, 1);
            continue;
        }

        err_t err = netconn_accept(nc, &printf_client);

        if (err == ERR_OK) {
            telnet_printf("Connected\n");
            netconn_close(nc);
            netconn_delete(nc);
            nc = NULL;

            struct netbuf *nb;
            while (netconn_recv(printf_client, &nb) == ERR_OK);

            //telnet_printf("Closing connection\n");
            netconn_close(printf_client);
            netconn_delete(printf_client);
            printf_client = NULL;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

int telnet_printf(const char *fmt, ...) {
    va_list ap;
    int len = 0;
    if (printf_client) {
        va_start(ap, fmt);

        char buf[512];
        len = vsnprintf(buf, sizeof (buf), fmt, ap);
        netconn_write(printf_client, buf, len, NETCONN_COPY);

        va_end(ap);
    }
    return len;
}