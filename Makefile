VERSION  = 1.0
PREFIX   = /usr/local

X11INC   = /usr/X11R6/include
X11LIB   = /usr/X11R6/lib

CFLAGS   = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os \
           -I$(X11INC) -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
           -DVERSION=\"$(VERSION)\"
LDFLAGS  = -L$(X11LIB) -lX11

CC       = cc

all: swm

swm: swm.c swm.h
	$(CC) $(CFLAGS) -o $@ swm.c $(LDFLAGS)

clean:
	rm -f swm

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 swm $(DESTDIR)$(PREFIX)/bin/swm

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/swm

.PHONY: all clean install uninstall
