# -*- Makefile -*-

GIT_VERSION := $(shell git describe --abbrev=6 --dirty --always)
AM_CPPFLAGS = ${libnfnetlink_CFLAGS} ${libnetfilter_queue_CFLAGS} \
              ${libnl_CPPFLAGS} -Iinclude -Ilib
AM_CFLAGS   = -Wall -Wcast-align -Wcast-qual -DVERSION=\"$(GIT_VERSION)\"

bin_PROGRAMS = opennop/opennop
sbin_PROGRAMS = opennopd/opennopd

opennop_opennop_SOURCES = \
	opennop/opennop.c
opennop_opennop_LDADD = -lpthread

opennopd_opennopd_SOURCES = \
	lib/quicklz.c \
	opennopd/compression.c opennopd/csum.c opennopd/sockets.c \
	opennopd/help.c opennopd/logger.c opennopd/version.c \
	opennopd/opennopd.c opennopd/packet.c opennopd/queuemanager.c \
	opennopd/sessionmanager.c opennopd/signals.c opennopd/tcpoptions.c \
	opennopd/subsystems/fetcher.c opennopd/subsystems/healthagent.c \
	opennopd/subsystems/sessioncleanup.c opennopd/subsystems/counters.c  \
	opennopd/subsystems/worker.c opennopd/subsystems/memorymanager.c \
	opennopd/subsystems/climanager.c opennopd/subsystems/clicommands.c \
	opennopd/subsystems/ipc.c
	
opennopd_opennopd_LDADD = \
	-lcrypt -lcrypto -ldl -lpthread -luuid -lrt ${libnetfilter_queue_LIBS}
