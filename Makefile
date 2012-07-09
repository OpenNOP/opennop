CC=gcc
CFLAGS=-c -Wall -Wcast-align -Wcast-qual
LDFLAGS=-lnfnetlink -lnetfilter_queue -lpthread -lnl
SOURCES=opennopd.c
OBJECTS=$(patsubst %.c,%.o,$(wildcard *.c)) $(patsubst %.c,%.o,$(wildcard */*.c))
EXECUTABLE=opennopd
DESTDIR?=/usr/local/bin

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	
.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(OBJECTS)
	rm $(EXECUTABLE)
	
install:
	mkdir -p $(DESTDIR)
	install $(EXECUTABLE) $(DESTDIR)/$(EXECUTABLE)