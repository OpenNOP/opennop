
override MODULE = opennopdrv
OBJS = module.o

MODULE_EXT	:= ko
$(MODULE)-y	:= $(OBJS)

obj-m += $(MODULE).o

KDIR    := $(INSTALL_MOD_PATH)/lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)


all modules:        
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	
clean:        
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	