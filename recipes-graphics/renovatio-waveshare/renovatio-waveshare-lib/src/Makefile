# renovatio-waveshare-lib
#
# Builds librenovatio-waveshare.a from our sources plus the upstream
# Waveshare C driver (https://github.com/waveshare/e-Paper), whose
# RaspberryPi_JetsonNano/c directory must be given as WAVESHARE_DIR.

WAVESHARE_DIR ?= waveshare/RaspberryPi_JetsonNano/c

CC ?= cc
AR ?= ar
CFLAGS ?= -O2
PKG_CONFIG ?= pkg-config

PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
VERSION = 1.0

LIB = librenovatio-waveshare.a
PC = renovatio-waveshare.pc

WS_CONFIG = $(WAVESHARE_DIR)/lib/Config
WS_EPD = $(WAVESHARE_DIR)/lib/e-Paper
WS_FONTS = $(WAVESHARE_DIR)/lib/Fonts

PANELS = 1in64g 2in13g 2in15g 2in36g 2in66g 3in0g 4in37g 7in3g

SRCS = rw_display.c rw_draw.c rw_ttf.c rw_partial.c
WS_SRCS = $(WS_CONFIG)/DEV_Config.c \
          $(WS_CONFIG)/dev_hardware_SPI.c \
          $(foreach p,$(PANELS),$(WS_EPD)/EPD_$(p).c) \
          $(foreach s,8 12 16 20 24,$(WS_FONTS)/font$(s).c)

OBJDIR = obj
OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS)) \
       $(patsubst %.c,$(OBJDIR)/ws/%.o,$(notdir $(WS_SRCS)))

RW_CPPFLAGS = -DRPI -DUSE_BCM2835_LIB -Iinclude \
              -I$(WS_CONFIG) -I$(WS_EPD) -I$(WS_FONTS)
RW_CFLAGS = -Wall -ffunction-sections -fdata-sections
FT_CFLAGS := $(shell $(PKG_CONFIG) --cflags freetype2)

vpath %.c $(WS_CONFIG) $(WS_EPD) $(WS_FONTS)

.PHONY: all clean install

all: $(LIB) $(PC)

$(LIB): $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(OBJDIR)/%.o: %.c include/renovatio-waveshare.h rw_internal.h
	@mkdir -p $(dir $@)
	$(CC) $(RW_CPPFLAGS) $(RW_CFLAGS) -Wextra $(FT_CFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Upstream code: build without -Wextra, it is not warning-clean.
$(OBJDIR)/ws/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(RW_CPPFLAGS) $(RW_CFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(PC): renovatio-waveshare.pc.in
	sed -e 's|@PREFIX@|$(PREFIX)|' -e 's|@LIBDIR@|$(LIBDIR)|' \
	    -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|' -e 's|@VERSION@|$(VERSION)|' \
	    $< > $@

install: all
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig $(DESTDIR)$(INCLUDEDIR)
	install -m 0644 $(LIB) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(PC) $(DESTDIR)$(LIBDIR)/pkgconfig/
	install -m 0644 include/renovatio-waveshare.h $(DESTDIR)$(INCLUDEDIR)/

clean:
	rm -rf $(OBJDIR) $(LIB) $(PC)
