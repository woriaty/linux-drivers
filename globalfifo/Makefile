KVERS = $(shell uname -r)

KERNEL_HEARDERS_HATH = /usr/src/linux-headers-$(KVERS)

obj-m += globalfifo.o

build: kernel_modules

kernel_modules:
	make -C $(KERNEL_HEARDERS_HATH) M=$(CURDIR) modules
clean:
	make -C $(KERNEL_HEARDERS_HATH) M=$(CURDIR) clean
