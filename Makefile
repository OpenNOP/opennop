CC=gcc
PWD=$(shell pwd)
INCLUDE_DIR= $(PWD)/include
CFLAGS=-c -Wall -Wcast-align -Wcast-qual
libnfnetlink_LIBS=`pkg-config --libs libnfnetlink`
libnfnetlink_CFLAGS=`pkg-config --cflags libnfnetlink`
libnetfilter_queue_LIBS=`pkg-config --libs libnetfilter_queue`
libnetfilter_queue_CFLAGS=`pkg-config --cflags libnetfilter_queue`
libnl_LIBS=`pkg-config --libs libnl-1`
libnl_CFLAGS=`pkg-config --cflags libnl-1`
ncurses_LIBS=`pkg-config --libs ncurses`
ncurses_CFLAGS=`pkg-config --cflags ncurses`
OPENNOPDLDFLAGS=-lpthread -lcrypt $(libnfnetlink_LIBS) $(libnetfilter_queue_LIBS) $(libnl_LIBS)
OPENNOPDCFLAGS=$(libnfnetlink_CFLAGS) $(libnetfilter_queue_CFLAGS) $(libnl_CFLAGS)
OPENNOPLDFLAGS= -lpthread $(ncurses_LIBS)
OPENNOPCFLAGS=$(ncurses_CFLAGS)
SOURCES=opennop/opennop opennopd/opennopd
OPENNOPD_OBJS=$(patsubst %.c,%.o,$(wildcard opennopd/*.c)) $(patsubst %.c,%.o,$(wildcard opennopd/subsystems/*.c)) $(patsubst %.c,%.o,$(wildcard opennopd/bcl/*.c)) $(patsubst %.c,%.o,$(wildcard opennopd/libcli/*.c))
OPENNOP_OBJS=$(patsubst %.c,%.o,$(wildcard opennop/*.c)) $(patsubst %.c,%.o,$(wildcard opennop/subsystems/*.c))
OPENNOP=opennop
OPENNOPD=opennopd
DESTDIR?=/usr/local/bin

all: $(SOURCES)

opennop/opennop: $(OPENNOP_OBJS) $(OPENNOP)
	$(CC) $(OPENNOPCFLAGS) -I $(INCLUDE_DIR) $(OPENNOP_OBJS) -o $@ $(OPENNOPLDFLAGS)

opennopd/opennopd: $(OPENNOPD_OBJS) $(OPENNOPD)
	$(CC) $(OPENNOPDCFLAGS) -I $(INCLUDE_DIR) $(OPENNOPD_OBJS) -o $@ $(OPENNOPDLDFLAGS)
	
.c.o:
	$(CC) -I $(INCLUDE_DIR) $(CFLAGS) $< -o $@

clean:
	rm $(OPENNOP_OBJS)
	rm $(OPENNOPD_OBJS)
	rm $(SOURCES)
	
install:
	mkdir -p $(DESTDIR)
	install opennop/opennop $(DESTDIR)/opennop
	install opennopd/opennopd $(DESTDIR)/opennopd