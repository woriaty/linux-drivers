KVERS = $(shell uname -r)

obj-m += hello_world.o

#KERNEL_PATH = /usr/src/linux-4.12
KERNEL_PATH = /usr/src/linux-headers-4.12.0-041200-generic

build:kernel_modules

kernel_modules:
	make -C $(KERNEL_PATH) M=$(CURDIR) modules

clean:
	make -C $(KERNEL_PATH) M=$(CURDIR) clean
