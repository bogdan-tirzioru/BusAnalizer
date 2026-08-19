CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
CFLAGS += $(shell pkg-config --cflags libusb-1.0)
LDLIBS += $(shell pkg-config --libs libusb-1.0)

.PHONY: all clean

all: bulk_test

bulk_test: bulk_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f bulk_test
