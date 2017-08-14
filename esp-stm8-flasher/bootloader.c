#include "bootloader.h"
#include "spiffs_check.h"
#include "config.h"
#include <FreeRTOSConfig.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <esp_spiffs.h>
#include <spiffs.h>

#define POLL(__x) \
        do { \
            timeout = configCPU_CLOCK_HZ / 100; \
            while (__x) if (!timeout--) return 0; \
        } while (0)

#define WAIT_FOR_ACK() \
        if (!wait_for_ack()) { \
            SPIFFS_close(&fs, fd); \
            return 0; \
        }

/**
 * Calculate CRC-8-CCIT.
 * Polynomial: x^8 + x^2 + x + 1 (0x07)
 *
 * @param data input byte
 * @param crc initial CRC
 * @return CRC value
 */
static inline uint8_t crc8_update(uint8_t data, uint8_t crc) {
    crc ^= data;
    for (uint8_t i = 0; i < 8; i++)
        crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1;
    return crc;
}

static uint8_t crc8(uint8_t *data, uint8_t crc, uint16_t len) {
    for (uint16_t i = 0; i < len; i++)
        crc = crc8_update(data[i], crc);
    return crc;
}

static void uart_write_block(uint8_t *buf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++)
        uart_putc(0, buf[i]);
}

static int wait_for_ack() {
    uint32_t timeout;
    int rx;
    POLL((rx = uart_getc_nowait(0)) < 0);
    if (rx != 0xAA) return 0;
    POLL((rx = uart_getc_nowait(0)) < 0);
    if (rx != 0xBB) return 0;
    return 1;
}

int bootloader_upload(const char *filename) {
    spiffs_file fd = SPIFFS_open(&fs, filename, SPIFFS_RDONLY, 0);
    SPIFFS_CHECK(fd);

    /* get file size */
    spiffs_stat s;
    int res = SPIFFS_fstat(&fs, fd, &s);
    SPIFFS_CHECK_SAFE(res, fd);
    uint16_t size = s.size;

    /* read file in chunks */
    uint8_t buf[BLOCK_SIZE];
    uint16_t total;
    uint8_t crc = 0;
    uint8_t chunks = 0;

    /* first pass - calculate CRC */
    for (total = size; total >= BLOCK_SIZE; total -= BLOCK_SIZE) {
        res = SPIFFS_read(&fs, fd, buf, BLOCK_SIZE);
        SPIFFS_CHECK_SAFE(res, fd);
        crc = crc8(buf, crc, BLOCK_SIZE);
        chunks++;
    }
    if (total) {
        /* last chunk */
        res = SPIFFS_read(&fs, fd, buf, total);
        SPIFFS_CHECK_SAFE(res, fd);
        /* pad with 0xFF */
        memset(&buf[total], 0xFF, BLOCK_SIZE - total);
        crc = crc8(buf, crc, BLOCK_SIZE);
        chunks++;
    }

    /* second pass - write firmware */
    SPIFFS_lseek(&fs, fd, 0, SPIFFS_SEEK_SET); // rewind
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, chunks, crc, crc};
    uart_write_block(payload, sizeof (payload));
    for (total = size; total >= BLOCK_SIZE; total -= BLOCK_SIZE) {
        WAIT_FOR_ACK();
        res = SPIFFS_read(&fs, fd, buf, BLOCK_SIZE);
        SPIFFS_CHECK_SAFE(res, fd);
        uart_write_block(buf, BLOCK_SIZE);
    }
    if (total) {
        /* last chunk */
        WAIT_FOR_ACK();
        res = SPIFFS_read(&fs, fd, buf, total);
        SPIFFS_CHECK_SAFE(res, fd);
        /* pad with 0xFF */
        memset(&buf[total], 0xFF, BLOCK_SIZE - total);
        uart_write_block(buf, BLOCK_SIZE);
    }

    WAIT_FOR_ACK();

    if (fd >= 0) {
        res = SPIFFS_close(&fs, fd);
        SPIFFS_CHECK(res);
    }

    return 1;
}
