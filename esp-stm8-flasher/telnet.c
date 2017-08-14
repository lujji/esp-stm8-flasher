#include "telnet.h"
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <lwip/api.h>
#include "config.h"
#include "telnet_printf.h"

static uint8_t rx_buf[128];

void telnet_task(void *pvParameter)
{
	struct netconn *nc = NULL, *client = NULL;

	while (1) {
		if (!nc) {
			nc = netconn_new(NETCONN_TCP);
			if (!nc) {
				//telnet_printf("Failed to allocate socket.\n");
				printf("Failed to allocate socket.\n");
				continue;
			}
			netconn_bind(nc, IP_ADDR_ANY, TELNET_PORT);
			netconn_listen(nc);
			continue;
		}

		err_t err = netconn_accept(nc, &client);
		netconn_set_recvtimeout(client, 5);

		if (err == ERR_OK) {
			telnet_printf("Connected\n");
			netconn_close(nc);
			netconn_delete(nc);
			nc = NULL;

			struct netbuf *nb;
			for (;;) {
				uint16_t rx_count = FIELD2VAL(UART_STATUS_RXFIFO_COUNT, UART(0).STATUS);

				if (rx_count) {
					printf("count: %d\n", rx_count);
					for (uint16_t i = 0; i < rx_count; i++)
						rx_buf[i] = (UART(0).FIFO);
					err = netconn_write(client, rx_buf, rx_count, NETCONN_COPY);
					if (err != ERR_OK) break;
					//taskYIELD();
				}

				err = netconn_recv(client, &nb);
				if (err == ERR_OK) {
					void *data;
					uint16_t len;
					netbuf_data(nb, &data, &len);
					for (uint16_t i = 0; i < len; i++)
						uart_putc(0, ((char *) data)[i]);
					netbuf_delete(nb);
					//taskYIELD();
				} else if (err != ERR_TIMEOUT) {
					break;
				}
			}

			telnet_printf("Closing connection\n");
			netconn_close(client);
			netconn_delete(client);
			client = NULL;
		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

	vTaskDelete(NULL);
}
