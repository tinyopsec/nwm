VERSION  = 0.2
PREFIX   = /usr/local
CFLAGS   = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os \
           -I/usr/include -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\"
LDFLAGS  = -L/usr/lib -lX11

CC       = cc

all: nwm
nwm: nwm.c nwm.h
	$(CC) $(CFLAGS) -o $@ nwm.c $(LDFLAGS)
clean:
	rm -f nwm
install: all
	install -Dm755 nwm $(DESTDIR)$(PREFIX)/bin/nwm
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/nwm
.PHONY: all clean install uninstall
