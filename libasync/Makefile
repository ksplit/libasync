PWD := `pwd`
KDIR := /lib/modules/`uname -r`/build
MODULE_DIR=module_build
LIB_DIR=lib_build

module:
	make -C $(KDIR) M=$(PWD)/module_build modules

clean:
	sudo rm -rf module_build/*.o
	sudo rm -rf module_build/*.ko
	sudo rm -rf module_build/*.symvers
	sudo rm -rf module_build/*.order
	sudo rm -rf module_build/*.mod.c
