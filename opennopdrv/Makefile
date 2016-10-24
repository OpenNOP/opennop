
override MODULE = opennopdrv
OBJS = module.o

MODULE_EXT	:= ko
$(MODULE)-y	:= $(OBJS)

obj-m	:= $(MODULE).o
KDIR    := /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)

genl_header := $(firstword $(wildcard \
  /usr/src/kernels/$(shell uname -r)/include/net/genetlink.h \
  /usr/src/linux/include/net/genetlink.h))

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
	