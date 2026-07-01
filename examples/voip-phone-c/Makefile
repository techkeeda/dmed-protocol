# DMED VoIP Phone Server — Makefile
#
# Cross-compile: make CROSS=/path/to/aarch64-ooma-linux-gnu-

CROSS    ?=
CC        = $(CROSS)gcc
SYSROOT  := $(shell $(CC) -print-sysroot 2>/dev/null)
PKG_CFG   = PKG_CONFIG_PATH=$(SYSROOT)/usr/lib/pkgconfig:$(SYSROOT)/usr/share/pkgconfig \
            PKG_CONFIG_SYSROOT_DIR=$(SYSROOT) pkg-config

CFLAGS    = -Wall -Os -I../../lib/c/include -I./dbus \
            $(shell $(PKG_CFG) --cflags glib-2.0 gio-2.0 gio-unix-2.0)
LDFLAGS   = -lpthread \
            $(shell $(PKG_CFG) --libs glib-2.0 gio-2.0 gio-unix-2.0)

TARGET    = voip_phone_dmed
SRCS      = voip_phone_dmed.c dmed_ble.c \
            dbus/objectmanager-generated.c \
            dbus/advertise-generated.c \
            dbus/service-generated.c \
            dbus/characteristic-generated.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS) dmed_ble.h
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
