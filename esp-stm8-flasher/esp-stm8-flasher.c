#include <string.h>
#include <stdio.h>
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
#include "telnet.h"
#include "telnet_printf.h"
#include "bootloader.h"

static QueueHandle_t qFlashWriter, qBootTask;

enum {
    ACK = 0xAABB,
    NACK = 0xDEAD,
};

struct RxPacket {
    char *fname;
    uint8_t *buf;
    uint16_t len;
    uint8_t type;
    struct tcp_pcb *pcb;
};

void flash_writer_task(void *pvParameter) {
    int res;
    struct RxPacket rx;
    static spiffs_file fd = -1;

    for (;;) {
        if (xQueueReceive(qFlashWriter, &rx, 0)) {
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

static void stm8_reset() {
    gpio_enable(RST_PIN, GPIO_OUTPUT);
    gpio_write(RST_PIN, false);
    vTaskDelay(1);
    gpio_write(RST_PIN, true);    
    gpio_disable(RST_PIN);
}

void bootloader_task(void *pvParameter) {
    struct RxPacket rx;

    for (;;) {
        if (xQueueReceive(qBootTask, &rx, 0)) {
            /* enter bootloader */
            gpio_enable(1, GPIO_OUTPUT);
            gpio_write(1, false);
            stm8_reset();
            vTaskDelay(1);
            gpio_disable(1);
            gpio_set_iomux_function(1, IOMUX_GPIO1_FUNC_UART0_TXD);
            uart_clear_rxfifo(0);
            uart_clear_txfifo(0);

            /* write fw */
            telnet_printf("writing fw..\n");
            int ok = bootloader_upload(rx.fname);
            
            uint16_t rsp = (ok) ? ACK : NACK;
            if (rx.pcb)
                websocket_write(rx.pcb, (uint8_t *) (&rsp), 2, WS_BIN_MODE);
        }
    }

    vTaskDelete(NULL);
}

/**
 * This function is called when websocket frame is received.
 */
void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, uint16_t data_len, uint8_t mode) {
    static uint16_t file_size, rx_size;
    static char fname[32];
    char fsize[16];
    
    char *sptr = (char *) data + 1;
    char *ptr;
    int res, len;
    
    uint16_t rsp = NACK;
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
            xQueueSend(qFlashWriter, &packet, (TickType_t) 1000);
            rsp = ACK;
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

            rsp = ACK;
            if (xQueueSend(qFlashWriter, &packet, (TickType_t) 1000) != pdPASS) {
                free(packet.buf);
                rsp = NACK;
            }
            break;
        case 'I':
            telnet_printf("[info]\n");
            spiffs_DIR d;
            struct spiffs_dirent e;
            struct spiffs_dirent *pe = &e;

            SPIFFS_opendir(&fs, "/", &d);
            char buf[64];
            while ((pe = SPIFFS_readdir(&d, pe))) {
                telnet_printf("%s [%04x] size:%i\n", pe->name, pe->obj_id, pe->size);
                int len = snprintf(buf, sizeof (buf),
                        "%s:%d,", (char *) pe->name, pe->size);
                if (len < sizeof (buf))
                    websocket_write(pcb, (uint8_t *) buf, len, WS_TEXT_MODE);
            }
            rsp = ACK;
            websocket_write(pcb, (uint8_t *) (&rsp), 2, WS_BIN_MODE);
            res = SPIFFS_closedir(&d);
            SPIFFS_CHECK_INF(res);
            return;
            break;
        case 'F':
            telnet_printf("[flash]\n");
            strlcpy(fname, (char *) &data[1], sizeof (fname));
            packet.fname = fname;
            packet.pcb = pcb;
            xQueueSend(qBootTask, &packet, (TickType_t) 100);
            return;
            break;
        case 'D':
            telnet_printf("[delete]\n");
            strlcpy(fname, (char *) &data[1], data_len);
            telnet_printf("deleting file: %s\n", fname);
            res = SPIFFS_remove(&fs, fname);
            SPIFFS_CHECK_INF(res);
            rsp = (res >= 0) ? ACK : NACK;
            break;
        default:
            telnet_printf("Unknown command\n");
            rsp = NACK;
            break;
    }

    websocket_write(pcb, (uint8_t *) (&rsp), 2, WS_BIN_MODE);
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
    disable_stdout();
    uart_set_baud(0, 115200);

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

    /* initialize SPIFFS */
    esp_spiffs_init();
    if (esp_spiffs_mount() != SPIFFS_OK) {
        SPIFFS_format(&fs);
        esp_spiffs_mount();
    }

    /* initialize queues */
    qFlashWriter = xQueueCreate(8, sizeof (struct RxPacket));
    qBootTask = xQueueCreate(1, sizeof (struct RxPacket));

    /* initialize tasks */
    xTaskCreate(&flash_writer_task, "Flash Writer Task", 512, NULL, 2, NULL);
    xTaskCreate(&bootloader_task, "Bootloader Task", 1024, NULL, 2, NULL);
    //xTaskCreate(&telnet_printf_task, "Telnet Print Task", 2048, NULL, 2, NULL);
    xTaskCreate(&telnet_task, "Telnet Task", 512, NULL, 2, NULL);
    xTaskCreate(&httpd_task, "HTTP Daemon", 128, NULL, 2, NULL);
}
