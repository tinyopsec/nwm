.POSIX:

NAME     = <NAME>
SRCS     = <LIST OF .c FILES>
HDRS     = <LIST OF .h FILES>
OBJS     = $(SRCS:.c=.o)

PREFIX   ?= /usr/local
CC       ?= c99
CFLAGS   ?= -O2 -Wall
CPPFLAGS ?= 
LDFLAGS  ?= 
LDLIBS   ?= <e.g. -lX11>

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

clean:
	rm -f $(NAME) $(OBJS)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)

.PHONY: all clean install uninstall
