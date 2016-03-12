LIB_TEST_ASYNC_DIR=$(PWD)
KDIR = /lib/modules/`uname -r`/build

export LIB_TEST_ASYNC_DIR

module:
	make -C $(KDIR) M=$(LIB_TEST_ASYNC_DIR) modules

clean:
	sudo rm -rf *.o
	sudo rm -rf *.ko
	sudo rm -rf *.symvers
	sudo rm -rf *.order
	sudo rm -rf *.mod.c
