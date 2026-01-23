MODULE_NAME := microbus_uart

obj-m := $(MODULE_NAME).o

$(MODULE_NAME)-objs := \
	src/microbus/uart.o \
	../common/src/common/crc8.o \
	../common/src/common/protocol/request.o \
	../common/src/common/protocol/response.o \
	../common/src/common/protocol/packet.o \
	../common/src/common/protocol/packet/decoder.o \
	../common/src/common/protocol/common.o

ccflags-y += -I$(PWD)/../common/include
# ccflags-y += -fsanitize=kernel-address

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
