LIBASYNC_DIR=$(PWD)
KDIR = /lib/modules/`uname -r`/build
MODULE_DIR=module_build
LIB_DIR=lib_build

export LIBASYNC_DIR

lib:
	make -C $(KDIR) M=$(LIBASYNC_DIR)/lib_build modules

module:
	make -C $(KDIR) M=$(LIBASYNC_DIR)/module_build modules

clean:
	sudo rm -rf module_build/*.o
	sudo rm -rf module_build/*.ko
	sudo rm -rf module_build/*.symvers
	sudo rm -rf module_build/*.order
	sudo rm -rf module_build/*.mod.c
	sudo rm -rf lib_build/*.o
	sudo rm -rf lib_build/*.ko
	sudo rm -rf lib_build/*.symvers
	sudo rm -rf lib_build/*.order
	sudo rm -rf lib_build/*.mod.c
	cd ../fast-ipc-module/current/tests/rpc && make clean
