mkdir fipc-build fipc-install
cd fipc-build;
KERNEL=/local/devel/lcd-kernel

../fast-ipc-module/configure --with-kernel-headers=${KERNEL} --prefix=`pwd`/../fipc-install

make && make install
cd ..;
mkdir async-build async-install
cd async-build;
../configure --with-kernel-headers=${KERNEL} --prefix=`pwd`/../async-install --with-libfipc=`pwd`/../fipc-install --enable-kernel-tests-build

make && make install
cd ../
