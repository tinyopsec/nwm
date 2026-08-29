# nwm - Nano window manager
# See LICENSE file for copyright and license details.

VERSION = 1.5

PREFIX  = /usr/local
BINDIR  = ${PREFIX}/bin

X11INC  = /usr/X11R6/include
X11LIB  = /usr/X11R6/lib

# Linux
#X11INC = /usr/include/X11
#X11LIB = /usr/lib

# FreeBSD / DragonFly
#X11INC = /usr/local/include
#X11LIB = /usr/local/lib

# NetBSD (pkgsrc)
#X11INC = /usr/pkg/include
#X11LIB = /usr/pkg/lib

INCS     = -I${X11INC}
LIBS     = -L${X11LIB} -lX11

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L \
           -DVERSION=\"${VERSION}\"
CFLAGS   = -std=c99 -pedantic -Wall -Wextra \
           -Wstrict-prototypes -Wmissing-prototypes -Wshadow \
           -fno-common -O2 -flto ${INCS} ${CPPFLAGS}
LDFLAGS  = -flto ${LIBS}

# Debug: make debug
DBGFLAGS = -std=c99 -pedantic -Wall -Wextra \
           -Wstrict-prototypes -Wmissing-prototypes -Wshadow \
           -fno-common -g -O0 ${INCS} ${CPPFLAGS}

CC = cc

SRC = nwm.c
OBJ = ${SRC:.c=.o}

all: nwm

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: nwm.h

nwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

debug: ${SRC} nwm.h
	${CC} ${DBGFLAGS} -o nwm_dbg ${SRC} ${LIBS}

dist: clean
	mkdir -p nwm-${VERSION}
	cp -R nwm.c nwm.h Makefile LICENSE nwm-${VERSION}
	tar -czf nwm-${VERSION}.tar.gz nwm-${VERSION}
	rm -rf nwm-${VERSION}

clean:
	rm -f nwm nwm_dbg ${OBJ} nwm-${VERSION}.tar.gz

install: all
	mkdir -p ${DESTDIR}${BINDIR}
	install -m 755 nwm ${DESTDIR}${BINDIR}/nwm

uninstall:
	rm -f ${DESTDIR}${BINDIR}/nwm

.PHONY: all clean debug dist install uninstall
