PROGRAM=esp-stm8-uploader
ESPBAUD=2000000

EXTRA_CFLAGS=-DLWIP_HTTPD_CGI=1 -DLWIP_HTTPD_SSI=1 -DWS_TIMEOUT=2 -I./fsdata

#Enable debugging
#EXTRA_CFLAGS+=-DLWIP_DEBUG=1 -DHTTPD_DEBUG=LWIP_DBG_ON
EXTRA_COMPONENTS=extras/mbedtls extras/httpd extras/spiffs
FLASH_SIZE = 32
SPIFFS_BASE_ADDR = 0x200000
SPIFFS_SIZE = 0x010000

include ./esp-open-rtos/common.mk

html:
	@echo "Generating fsdata.."
	cd fsdata && ./makefsdata
