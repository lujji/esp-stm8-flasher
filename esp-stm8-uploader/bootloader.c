#include "bootloader.h"
#include "spiffs_check.h"
#include "config.h"
#include "misc.h"
#include <spiffs.h>
#include <esp_spiffs.h>

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

int read_file(const char *filename) {
    spiffs_file fd = SPIFFS_open(&fs, filename, SPIFFS_RDONLY, 0);
    SPIFFS_CHECK(fd);

    /* get file size */
    spiffs_stat s;
    int res = SPIFFS_fstat(&fs, fd, &s);
    SPIFFS_CHECK_SAFE(res, fd);
    uint16_t size = s.size;
    telnet_printf("Size = %d\n", size);

    /* read file in chunks */
    uint8_t buf[BLOCK_SIZE];
    uint16_t total;
    uint8_t crc = 0;
    uint8_t chunks = 0;

    /* first pass -calculate CRC */
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
    telnet_printf("CRC = %02X\n", crc);

    /* generate payload */
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, chunks, crc, crc};

    /* second pass - write firmware */
    SPIFFS_lseek(&fs, fd, 0, SPIFFS_SEEK_SET); // rewind
    for (total = size; total >= BLOCK_SIZE; total -= BLOCK_SIZE) {
        res = SPIFFS_read(&fs, fd, buf, BLOCK_SIZE);
        SPIFFS_CHECK_SAFE(res, fd);
        hexdump16(buf, BLOCK_SIZE);
    }
    if (total) {
        /* last chunk */
        telnet_printf("..%d bytes remaining\n", total);
        res = SPIFFS_read(&fs, fd, buf, total);
        SPIFFS_CHECK_SAFE(res, fd);
        /* pad with 0xFF */
        memset(&buf[total], 0xFF, BLOCK_SIZE - total);
        hexdump16(buf, BLOCK_SIZE);
    }

    if (fd >= 0) {
        telnet_printf("Closing..\n");
        res = SPIFFS_close(&fs, fd);
        SPIFFS_CHECK(res);
    }
    telnet_printf("done\n");

    return 1;
}
