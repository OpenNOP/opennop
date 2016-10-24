
override MODULE = opennopdrv
OBJS = module.o

MODULE_EXT	:= ko
$(MODULE)-y	:= $(OBJS)

obj-m	:= $(MODULE).o
K_PATH	:= $(shell \
	moddir=`find /lib/modules -maxdepth 1 -type d -regex '.*[0-9]\.*' -print -quit`; \
	if [ -d "/lib/modules/`uname -r`" ]; then echo "/lib/modules/`uname -r`"; \
	elif [ -d "$moddir" ]; then echo "$moddir"; \
	fi)
KDIR 	:= $(K_PATH)/build
PWD     := $(shell pwd)


all modules:        
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	
clean:        
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	
