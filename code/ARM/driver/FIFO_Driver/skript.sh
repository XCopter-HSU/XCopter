make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C <kernel-source-path> M=`pwd` modules

arm-linux-gnueabihf-strip -R .comment -R .note -g --strip-unneeded FifoDriver.ko



