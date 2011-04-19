CC=gcc
CFLAGS=-c -Wall -Wcast-align
LDFLAGS=-lnfnetlink -lnetfilter_queue -lpthread -lnl
SOURCES=daemon.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=opennopd

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	
.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(OBJECTS)
	rm $(EXECUTABLE)
	
install:
	install $(EXECUTABLE) /usr/local/bin/$(EXECUTABLE)