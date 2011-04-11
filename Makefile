all:
	gcc daemon.c -o opennopd -lnfnetlink -lnetfilter_queue -lpthread -lnl
	
clean:
	rm opennopd
	
install:
	install opennopd /usr/local/bin/opennopd