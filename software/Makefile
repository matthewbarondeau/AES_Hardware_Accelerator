#!/bin/sh

CC := arm-linux-gnueabihf-gcc

# obj-m is a list of what kernel modules to build.  The .o and other
# objects will be automatically built from the corresponding .c file -
# no need to list the source files explicitly.

obj-m := acc_interrupt.o

# KDIR is the location of the kernel source.  The current standard is
# to link to the associated source tree from the directory containing
# the compiled modules.


KDIR	:= /usr/src/plnx_kernel


# PWD is the current working directory and the location of our module
# source files.

PWD   := $(shell pwd)

# default is the default make target.  The rule here says to run make
# with a working directory of the directory containing the kernel
# source and compile only the modules in the PWD (local) directory.

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(CC)  aes_test.c -o aes_test
#	$(CC)  no_int_sha.c -o no_int_sha
#	$(CC)  test_interrupt.c -o test_interrupt
#	$(CC)  intr_latency_csv.c -o intr_latency_csv

clean::
	$(RM) .skeleton* *.cmd *.o *.ko *.mod.c *.exe test_dma
	$(RM) -R .tmp*

