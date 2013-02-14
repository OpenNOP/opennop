CC=gcc
PWD=$(shell pwd)
INCLUDE_DIR= $(PWD)/include
CFLAGS=-c -Wall -Wcast-align -Wcast-qual
OPENNOPDLDFLAGS=`pkg-config --libs libnfnetlink` `pkg-config --libs libnetfilter_queue` `pkg-config --libs libnl-1` -lpthread -lcrypt
OPENNOPDCFLAGS=`pkg-config --cflags libnfnetlink` `pkg-config --cflags libnetfilter_queue` `pkg-config --cflags libnl-1`
OPENNOPLDFLAGS=`pkg-config --libs ncurses` -lpthread
OPENNOPCFLAGS=`pkg-config --cflags ncurses`
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