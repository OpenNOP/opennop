
override MODULE = opennopdrv
OBJS = module.o

MODULE_EXT	:= ko
$(MODULE)-y	:= $(OBJS)

obj-m	:= $(MODULE).o
KDIR    := /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)

genl_header := $(strip \
	$(shell if [ -f "/usr/src/kernels/`uname -r`/include/net/genetlink.h" ]; then echo "/usr/src/kernels/`uname -r`/include/net/genetlink.h"; fi) \
	$(shell if [ -f "/usr/src/linux/include/net/genetlink.h" ]; then echo "/usr/src/linux/include/net/genetlink.h"; fi))

genl_version := $(strip \
  $(shell grep -q "define genl_register_family_with_ops" $(genl_header) && echo __MY_NL_NEWEST__) \
  $(shell grep -q "extern int genl_register_family_with_ops" $(genl_header) && echo __MY_NL_NEWER__) \
  $(shell grep -q "extern int genl_register_ops" $(genl_header) && echo __MY_NL_OLDEST__))
ifeq ($(genl_version),)
  $(error "error")
endif

ccflags-y   += -D __MY_NL_VERSION__=$(genl_version) -D __MY_NL_NEWEST__=3 -D __MY_NL_NEWER__=2 -D __MY_NL_OLDEST__=1


all modules:        
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	
clean:        
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	