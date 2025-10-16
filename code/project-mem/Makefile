# Patched Makefile: prefer pkg-config; fall back to /usr/local
CC ?= gcc
CFLAGS_COMMON := -O2 -Wall -Wextra -std=gnu11

LIBVMI_CFLAGS := $(shell pkg-config --cflags libvmi 2>/dev/null)
LIBVMI_LIBS   := $(shell pkg-config --libs libvmi 2>/dev/null)

ifeq ($(strip $(LIBVMI_CFLAGS)),)
  LIBVMI_CFLAGS := -I/usr/local/include
endif
ifeq ($(strip $(LIBVMI_LIBS)),)
  LIBVMI_LIBS := -L/usr/local/lib -lvmi
endif

CAPSTONE_LIBS := -lcapstone

CFLAGS := $(CFLAGS_COMMON) $(LIBVMI_CFLAGS)
LDFLAGS := $(LIBVMI_LIBS) $(CAPSTONE_LIBS)

BIN = libvmi_heap_write_tracer
OBJS = main.o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BIN) $(OBJS)

.PHONY: all clean
