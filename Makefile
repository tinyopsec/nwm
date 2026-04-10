# swm - simple window manager
# See LICENSE file for copyright and license details.

VERSION = 1.0

PREFIX  = /usr/local
MANPREFIX = ${PREFIX}/share/man

INCS = -I/usr/X11R6/include
LIBS = -L/usr/X11R6/lib -lX11

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L -DVERSION=\"${VERSION}\"
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

CC = cc

SRC = swm.c
OBJ = ${SRC:.c=.o}

all: swm

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: swm.h

swm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f swm ${OBJ} swm-${VERSION}.tar.gz

dist: clean
	mkdir -p swm-${VERSION}
	cp -R LICENSE Makefile README swm.h ${SRC} swm-${VERSION}
	tar -cf swm-${VERSION}.tar swm-${VERSION}
	gzip swm-${VERSION}.tar
	rm -rf swm-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f swm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/swm

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/swm

.PHONY: all clean dist install uninstall
