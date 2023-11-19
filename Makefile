.PHONY: build
KDIR ?= /lib/modules/`uname -r`/build
build:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
install: build
	sudo insmod ktest_hello.ko
uninstall:
	sudo rmmod ktest_hello

main: ./test/main.c
	$(CC) -o main ./test/main.c