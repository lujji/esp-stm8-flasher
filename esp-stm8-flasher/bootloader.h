#ifndef BOOTLOADER_H
#define BOOTLOADER_H

/**
 * Perform firmware update
 * @param filename
 * @return 1 = update successful, 0 = fail
 */
int bootloader_upload(const char *filename);

#endif /* BOOTLOADER_H */
