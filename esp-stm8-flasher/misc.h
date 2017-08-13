#ifndef MISC_H
#define MISC_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MISC_PRINTF(...) do { printf(__VA_ARGS__); } while (0);
#define MISC_PUTCHAR(c)  do { putchar(c); } while (0);

#define for_each(__item__, __array__) \
    for(int __keep__ = 1, \
            __count__ = 0,\
            __size__ = sizeof (__array__) / sizeof *(__array__); \
        __keep__ && __count__ != __size__; \
        __keep__ = !__keep__, __count__++) \
      for(__item__ = (__array__) + __count__; __keep__; __keep__ = !__keep__)

#define for_arr(__array__) \
    for(int __keep__ = 1, \
            i = 0,\
            __size__ = sizeof (__array__) / sizeof *(__array__); \
        __keep__ && i != __size__; \
        __keep__ = !__keep__, i++) \
      for(; __keep__; __keep__ = !__keep__)

#define for_i(__array__, __size__) \
    for(int __keep__ = 1, i = 0; \
        __keep__ && i != __size__; \
        __keep__ = !__keep__, i++) \
      for(; __keep__; __keep__ = !__keep__)

static inline void hexdump(uint8_t *buf, size_t len, int delim) {
    for (size_t i = 0; i < len; i++) {
        if (i % delim == 0) MISC_PUTCHAR('\n');
        MISC_PRINTF("%02X ", buf[i]);
    }
    MISC_PUTCHAR('\n');
}

static inline void hexdump16(uint8_t *buf, size_t len) {
    hexdump(buf, len, 16);
}

static inline void hexdump32(uint8_t *buf, size_t len) {
    hexdump(buf, len, 32);
}

static inline size_t file_load(const char *fname, uint8_t *buf, size_t len) {
    FILE *fp = fopen(fname, "rb");
    if (fp == NULL) {
        MISC_PRINTF("File not found\n");
        return 0;
    }

    size_t size = fread(buf, 1, len, fp);
    MISC_PRINTF("Read %d bytes\n", size);

    if (fclose(fp) != 0)
        MISC_PRINTF("Could not close file\n");

    return size;
}

static inline int file_save(const char *fname, uint8_t *buf, size_t len) {
    FILE *fp = fopen(fname, "wb+");
    if (fp == NULL) {
        MISC_PRINTF("Could not create file!\n");
        return 0;
    }

    fwrite(buf, len, 1, fp);
    if (fclose(fp) != 0)
        MISC_PRINTF("Could not close file\n");

    return 1;
}

#endif /* MISC_H */
