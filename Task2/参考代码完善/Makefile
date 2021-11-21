ifneq ($(KERNELRELEASE),)
obj-m := BlockFIFODev.o
else
KDIR := /usr/src/linux-headers-$(shell uname -r)
all:
	make -w -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
endif