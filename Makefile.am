# -*- Makefile -*-

AM_CPPFLAGS = ${libnfnetlink_CFLAGS} ${libnetfilter_queue_CFLAGS} \
              ${libnl_CPPFLAGS} -Iinclude
AM_CFLAGS   = -Wall -Wcast-align -Wcast-qual

bin_PROGRAMS = opennop/opennop
sbin_PROGRAMS = opennopd/opennopd

opennop_opennop_SOURCES = \
	opennop/opennop.c
opennop_opennop_LDADD = -lpthread

opennopd_opennopd_SOURCES = \
	opennopd/compression.c opennopd/csum.c \
	opennopd/help.c opennopd/logger.c \
	opennopd/opennopd.c opennopd/packet.c opennopd/queuemanager.c \
	opennopd/sessionmanager.c opennopd/signals.c opennopd/tcpoptions.c \
	opennopd/qlzc/quicklz.c opennopd/libcli/libcli.c \
	opennopd/subsystems/fetcher.c opennopd/subsystems/healthagent.c \
	opennopd/subsystems/sessioncleanup.c \
	opennopd/subsystems/worker.c opennopd/subsystems/memorymanager.c \
	opennopd/subsystems/climanager.c opennopd/subsystems/clicommands.c \
	opennopd/subsystems/counters.c opennopd/sqlite3.c
opennopd_opennopd_LDADD = \
	-lcrypt -ldl -lpthread ${libnetfilter_queue_LIBS}
