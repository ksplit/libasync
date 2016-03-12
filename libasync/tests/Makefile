LIB_TEST_ASYNC_DIR=$(PWD)
KDIR = /lib/modules/`uname -r`/build

export LIB_TEST_ASYNC_DIR

simple:
	make -C $(KDIR) M=$(LIB_TEST_ASYNC_DIR)/simple modules

clean:
	sudo rm -rf simple/*.o
	sudo rm -rf simple/*.ko
	sudo rm -rf simple/*.symvers
	sudo rm -rf simple/*.order
	sudo rm -rf simple/*.mod.c

.PHONY: simple clean
