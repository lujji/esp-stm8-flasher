#include "telnet_printf.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <lwip/sys.h>
#include <lwip/api.h>

static volatile bool write_pending = false;
static SemaphoreHandle_t xSemaphore = NULL;
static char buf[256];
static int len;

void telnet_printf_task(void *arg) {
    LWIP_UNUSED_ARG(arg);
    struct netconn *nc = NULL;
    struct netconn *printf_client = NULL;

    while (1) {
        if (!nc) {
            nc = netconn_new(NETCONN_TCP);
            if (!nc) {
                /* failed to allocate socket */
                break;
            }
            netconn_bind(nc, IP_ADDR_ANY, TELNET_PRINTF_PORT);
            netconn_listen(nc);
        }

        err_t err = netconn_accept(nc, &printf_client);

        if (err == ERR_OK) {
            netconn_close(nc);
            netconn_delete(nc);
            nc = NULL;

            write_pending = false;
            xSemaphore = xSemaphoreCreateMutex();
            telnet_printf("Welcome!\n");

            for (;;) {
                if (write_pending) {
                    write_pending = false;
                    err = netconn_write(printf_client, buf, len, NETCONN_COPY);
                    xSemaphoreGive(xSemaphore);
                    if (err != ERR_OK) break;
                }
                vTaskDelay((TickType_t) 100);
            }

            vSemaphoreDelete(xSemaphore);
            xSemaphore = NULL;

            /* close connection */
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
    if (xSemaphore != NULL && xSemaphoreTake(xSemaphore, (TickType_t) 10) == pdTRUE) {
        write_pending = true;
        va_start(ap, fmt);
        len = vsnprintf(buf, sizeof (buf), fmt, ap);
        va_end(ap);
    }
    return len;
}