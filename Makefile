CC=gcc
PWD=$(shell pwd)
INCLUDE_DIR= $(PWD)/include
CFLAGS=-c -Wall -Wcast-align -Wcast-qual
OPENNOPDLDFLAGS=-lnfnetlink -lnetfilter_queue -lpthread -lnl -lcrypt
OPENNOPLDFLAGS=-pthread -lncurses
SOURCES=opennop/opennop opennopd/opennopd
OPENNOPD_OBJS=$(patsubst %.c,%.o,$(wildcard opennopd/*.c)) $(patsubst %.c,%.o,$(wildcard opennopd/subsystems/*.c)) $(patsubst %.c,%.o,$(wildcard opennopd/bcl/*.c)) $(patsubst %.c,%.o,$(wildcard opennopd/libcli/*.c))
OPENNOP_OBJS=$(patsubst %.c,%.o,$(wildcard opennop/*.c)) $(patsubst %.c,%.o,$(wildcard opennop/subsystems/*.c))
OPENNOP=opennop
OPENNOPD=opennopd
DESTDIR?=/usr/local/bin

all: $(SOURCES)

opennop/opennop: $(OPENNOP_OBJS) $(OPENNOP)
	$(CC) -I $(INCLUDE_DIR) $(OPENNOP_OBJS) -o $@ $(OPENNOPLDFLAGS)

opennopd/opennopd: $(OPENNOPD_OBJS) $(OPENNOPD)
	$(CC) -I $(INCLUDE_DIR) $(OPENNOPD_OBJS) -o $@ $(OPENNOPDLDFLAGS)
	
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