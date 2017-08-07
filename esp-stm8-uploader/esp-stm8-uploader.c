#include <string.h>
#include <stdio.h>
#include <stdout_redirect.h>
#include <ssid_config.h>
#include <espressif/esp_common.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <spiffs.h>
#include <esp_spiffs.h>
#include <httpd/httpd.h>
#include <lwip/sys.h>
#include <lwip/api.h>

#include "config.h"
#include "misc.h"
#include "spiffs_check.h"
#include "telnet_printf.h"
#include "bootloader.h"

QueueHandle_t qHandle;

struct RxPacket {
    char *fname;
    uint8_t *buf;
    uint16_t len;
    uint8_t type;
};

void flash_writer_task(void *pvParameter) {
    int res;
    struct RxPacket rx;
    static spiffs_file fd = -1;

    for (;;) {
        if (xQueueReceive(qHandle, &rx, 0)) {
            if (rx.type == 0) {
                SPIFFS_clearerr(&fs);
                if (fd != -1) {
                    telnet_printf("File still open - closing..\n");
                    res = SPIFFS_close(&fs, fd);
                    SPIFFS_CHECK_INF(res);
                    fd = -1;
                }

                if (rx.fname) {
                    fd = SPIFFS_open(&fs, rx.fname,
                            SPIFFS_CREAT | SPIFFS_RDWR |
                            SPIFFS_APPEND | SPIFFS_TRUNC, 0);
                    SPIFFS_CHECK_INF(fd);
                }
            } else {
                if (rx.len > 0) {
                    telnet_printf("working..\n");
                    int res = SPIFFS_write(&fs, fd, rx.buf, rx.len);
                    free(rx.buf);
                    SPIFFS_CHECK_INF(res);

                    if (rx.type == 2) {
                        telnet_printf("done!\n");
                        res = SPIFFS_close(&fs, fd);
                        SPIFFS_CHECK_INF(res);
                        fd = -1;
                    }
                }
            }
        }
    }

    vTaskDelete(NULL);
}

void file_reader_task(void *pvParameter) {
    char *fname = (char *) pvParameter;
    read_file(fname);

    vTaskDelete(NULL);
}

/**
 * This function is called when websocket frame is received.
 *
 * Note: this function is executed on TCP thread and should return as soon
 * as possible.
 */
void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, uint16_t data_len, uint8_t mode) {
    const uint16_t ACK = 0xAABB;
    const uint16_t NACK = 0xDEAD;

    static uint16_t file_size, rx_size;
    static char fname[16];
    char fsize[16];

    uint8_t response[2];
    uint16_t val = NACK;

    char *ptr, *sptr = (char *) data + 1;
    int res, len;

    struct RxPacket packet;

    switch (data[0]) {
        case 0x0A: // start frame
            telnet_printf("[start]\n");
            fname[0] = '\0';
            fsize[0] = '\0';
            data[data_len] = '\0';
            ptr = strsep(&sptr, ";");
            if (ptr) strlcpy(fname, ptr, sizeof (fname));
            ptr = strsep(&sptr, ";");
            if (ptr) strlcpy(fsize, ptr, sizeof (fsize));
            file_size = atoi(fsize);
            rx_size = 0;

            telnet_printf("filename: %s\nfilesize: %s\n", fname, fsize);

            packet.fname = fname;
            packet.type = 0;
            xQueueSend(qHandle, &packet, (TickType_t) 1000);

            val = ACK;
            break;
        case 0x0B: // data frame
            telnet_printf("[data]\n");
            len = data_len - 1;
            rx_size += len;
            packet.buf = malloc(len);
            memcpy(packet.buf, &data[1], len);
            packet.len = len;
            packet.type = 1;

            if (rx_size >= file_size) {
                /* EOF - close file */
                telnet_printf("last frame\n");
                if (rx_size > file_size)
                    telnet_printf("Warning: received garbage bytes\n");
                packet.type = 2;
            }

            if (xQueueSend(qHandle, &packet, (TickType_t) 1000) != pdPASS) {
                free(packet.buf);
                val = NACK;
            }

            val = ACK;
            break;
        case 'I':
            telnet_printf("[info]\n");
            gpio_write(LED_PIN, true);
            //uart_putc(0, 0xBB);

            spiffs_DIR d;
            struct spiffs_dirent e;
            struct spiffs_dirent *pe = &e;

            SPIFFS_opendir(&fs, "/", &d);
            char rsp[64];
            while ((pe = SPIFFS_readdir(&d, pe))) {
                telnet_printf("%s [%04x] size:%i\n", pe->name, pe->obj_id, pe->size);
                int len = snprintf(rsp, sizeof (rsp),
                        "%s:%d,", (char *) pe->name, pe->size);
                if (len < sizeof (rsp))
                    websocket_write(pcb, (uint8_t *) rsp, len, WS_TEXT_MODE);
            }
            res = SPIFFS_closedir(&d);
            SPIFFS_CHECK_INF(res);
            return;
            break;
        case 'F':
            telnet_printf("[flash]\n");
            strlcpy(fname, (char *) &data[1], data_len);
            telnet_printf("file: %s\n", fname);
            xTaskCreate(&file_reader_task, "HTTP Daemon", 256, fname, 2, NULL);
            //read_file(fname);
            val = ACK;
            break;
        case 'D':
            telnet_printf("[delete]\n");
            strlcpy(fname, (char *) &data[1], data_len);
            telnet_printf("deleting file: %s\n", fname);
            res = SPIFFS_remove(&fs, fname);
            SPIFFS_CHECK_INF(res);
            val = (res >= 0) ? ACK : NACK;
            break;
        default:
            telnet_printf("Unknown command\n");
            val = NACK;
            break;
    }

    response[1] = (uint8_t) val;
    response[0] = val >> 8;

    websocket_write(pcb, response, 2, WS_BIN_MODE);
}

long schlong(struct _reent *r, int fd, const char *ptr, int len) {
    return 0;
}

void telnet_task(void *arg) {
    LWIP_UNUSED_ARG(arg);
    struct netconn *nc = NULL, *client = NULL;

    while (1) {
        if (!nc) {
            nc = netconn_new(NETCONN_TCP);
            if (!nc) {
                telnet_printf("Failed to allocate socket.\n");
                break;
            }
            netconn_bind(nc, IP_ADDR_ANY, TELNET_PORT);
            netconn_listen(nc);
            continue;
        }

        err_t err = netconn_accept(nc, &client);

        if (err == ERR_OK) {
            telnet_printf("Connected\n");
            netconn_close(nc);
            netconn_delete(nc);
            nc = NULL;

            struct netbuf *nb;
            while (netconn_recv(client, &nb) == ERR_OK) {
                void *data;
                uint16_t len;
                netbuf_data(nb, &data, &len);
                telnet_printf("RX\n");
                //telnet_printf("RX: %.*s\n", len, (char *) data);
                err = netconn_write(client, data, len, NETCONN_COPY);
                netbuf_delete(nb);
            }
            telnet_printf("Closing connection\n");
            netconn_close(client);
            netconn_delete(client);
            client = NULL;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void heartbeat_task(void *pvParameters) {
    int ctr = 0;
    for (;;) {
        telnet_printf("Test %d\n", ctr++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

char *about_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    return "/about.html";
}

void httpd_task(void *pvParameters) {
    tCGI pCGIs[] = {
        {"/about", (tCGIHandler) about_cgi_handler},
    };

    /* register handlers and start the server */
    http_set_cgi_handlers(pCGIs, sizeof (pCGIs) / sizeof (pCGIs[0]));
    websocket_register_callbacks(NULL, (tWsHandler) websocket_cb);
    httpd_init();

    for (;;);
}

void user_init(void) {
    //set_write_stdout(schlong);

    uart_set_baud(0, 115200);
    telnet_printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);
    sdk_wifi_station_connect();

    /* turn off LED */
    gpio_enable(LED_PIN, GPIO_OUTPUT);
    gpio_write(LED_PIN, true);

    esp_spiffs_init();
    if (esp_spiffs_mount() != SPIFFS_OK) {
        SPIFFS_format(&fs);
        if (esp_spiffs_mount() != SPIFFS_OK)
            telnet_printf("Error mount SPIFFS\n");
    }

    qHandle = xQueueCreate(8, sizeof (struct RxPacket));

    /* initialize tasks */
    xTaskCreate(&heartbeat_task, "Heartbeat", 512, NULL, 2, NULL);
    xTaskCreate(&flash_writer_task, "Flash Writer", 512, NULL, 2, NULL);
    xTaskCreate(&telnet_printf_task, "Telnet Info Task", 1024, NULL, 2, NULL);
    xTaskCreate(&telnet_task, "Telnet Task", 512, NULL, 2, NULL);
    xTaskCreate(&httpd_task, "HTTP Daemon", 128, NULL, 2, NULL);
}
