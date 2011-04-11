
override MODULE = opennopdrv
OBJS = module.o

MODULE_EXT	:= ko
$(MODULE)-y	:= $(OBJS)

obj-m += $(MODULE).o

KDIR    := /lib/modules/$(shell uname -r)/build
PWD             := $(shell pwd)


all modules:        
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:        
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm Module.symvers Module.markers
	
install:
	install -d -v /usr/local/lib/modules/`uname -r`/misc/
	install $(MODULE).ko /usr/local/lib/modules/`uname -r`/misc/$(MODULE).ko
	