Overview
--------

This is the AC/Threaded C (THC) code from the Barrelfish team, with
bug fixes and extensions for our own asynchronous ipc (using libfipc).

While the original Barrelfish code has support for other platforms
and other architectures, the current build system for libasync just
supports the Linux kernel on x86_64. (The build, etc. can be extended
if and when it's needed.)

Kernel Patch
------------

Note: If this is being used in the vanilla kernel, the following should 
be added to the end of the struct task_struct definition in 
include/linux/sched.h:

    struct ptstate_t *ptstate;

and you should add an initializer to include/linux/init_task.h. Finally,
you should re-build your kernel with CONFIG_LAZY_THC set.

Build Instructions
------------------

Assume your libasync repository is at absolute path ABS_PATH.

First, build libfipc. Make sure the git submodule has been pulled in; do:

      cd ABS_PATH
      git submodule update --init

Then, configure and build it:

      mkdir fipc-build
      mkdir fipc-install
      cd fast-ipc-module
      ./autogen.sh
      cd ../fipc-build
      ../fast-ipc-module/configure --with-kernel-headers=<path to kernel> \
              --prefix=ABS_PATH/fipc-install
      make
      make install

Next, configure and build libasync:

      cd ..
      mkdir async-build
      mkdir async-install
      ./autogen.sh
      cd async-build
      ../configure --with-kernel-headers=<path to kernel> \
              --prefix=ABS_PATH/async-install \
              --with-libfipc=ABS_PATH/fipc-install \
              --enable-kernel-tests-build
      make
      make install

This will install libasync.a in async-install/lib and the headers in
async-install/include. It will also put kernel module tests in the lib/
folder. Note that these tests depend on libfipc, and are statically linked
with it. So, you don't need to do any further linking after they are
built; they are ready to go. By default, libasync is in the lazy configuration.
If you want to use the eager configuration, pass the flag --enable-eager to the
libasync configure script in addition to the arguments already specified above.
