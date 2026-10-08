# renovatio-waveshare-clock

CC ?= cc
CFLAGS ?= -O2
PKG_CONFIG ?= pkg-config

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

TARGET = renovatio-waveshare-clock
SRCS = main.c config.c weather.c render.c
OBJS = $(SRCS:.c=.o)

PKGS = renovatio-waveshare libcjson libcurl
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKGS))
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKGS))

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(PKG_LIBS) -lm

%.o: %.c config.h render.h weather.h
	$(CC) -Wall -Wextra $(PKG_CFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/

clean:
	rm -f $(OBJS) $(TARGET)
