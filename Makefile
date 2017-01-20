obj-m := mychardriver.o
KDIR := /lib/modules/`uname -r`/build
all:
	make -C $(KDIR) SUBDIRS=`pwd` modules
clean:
	make -C $(KDIR) SUBDIRS=`pwd` clean
