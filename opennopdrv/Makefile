
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

genl_header := $(firstword $(wildcard \
  /usr/src/kernels/$(shell uname -r)/include/net/genetlink.h \
  /usr/src/linux/include/net/genetlink.h \
  /usr/src/linux-headers-$(shell uname -r)/include/net/genetlink.h))
  
genl_version := $(strip \
  $(shell grep -q "define genl_register_family_with_ops" $(genl_header) && echo HAVE_GENL_WITH_OPS_AUTOLEN) \
  $(shell grep -q "extern int genl_register_family_with_ops" $(genl_header) && echo HAVE_GENL_WITH_OPS) \
  $(shell grep -q "extern int genl_register_ops" $(genl_header) && echo HAVE_GENL))
ifeq ($(genl_version),)
  $(error "No genl_* function detected")
endif

ccflags-y += -D$(genl_version)


all modules:        
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	
clean:        
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	
