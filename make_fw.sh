OPENSBI_REPO="https://github.com/fgssfgss/opensbi.git"
LINUX_VERSION="5.18.1"
LINUX_SOURCE="https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-$LINUX_VERSION.tar.xz"
BUSYBOX_VERSION="1.35.0"
BUSYBOX_SOURCE="https://busybox.net/downloads/busybox-$BUSYBOX_VERSION.tar.bz2"

LINUX_CONFIG="./linux-config"
BUSYBOX_CONFIG="./busybox-config"

FW_BUILD_PATH=$(realpath ./initramfs_dir)

CROSS_COMPILER=riscv32-unknown-linux-gnu-

INIT_SCRIPT="#!/bin/busybox sh	                    \r\n\
mount -t devtmpfs  devtmpfs  /dev                   \r\n\
mount -t proc      proc      /proc                  \r\n\
mount -t sysfs     sysfs     /sys                   \r\n\
mount -t tmpfs     tmpfs     /tmp                   \r\n\
cat <<!                                             \r\n\
                                                    \r\n\
Boot took \$(cut -d' ' -f1 /proc/uptime) seconds    \r\n\
        _       _     __                            \r\n\
  /\/\ (_)_ __ (_)   / /(_)_ __  _   ___  __        \r\n\
 /    \| | '_ \| |  / / | | '_ \| | | \ \/ /        \r\n\
/ /\/\ \ | | | | | / /__| | | | | |_| |>  <         \r\n\
\/    \/_|_| |_|_| \____/_|_| |_|\__,_/_/\_\        \r\n\
                                                    \r\n\
Welcome to mini_linux                               \r\n\
!                                                   \r\n\
exec /bin/sh"                               

function die() 
{
    echo "Something goes wrong... :("
    exit 1
}

function build_sources()
{
	rm -rf $FW_BUILD_PATH || die
	mkdir -p $FW_BUILD_PATH || die 

    echo "Downloading..."
	wget $LINUX_SOURCE -P $FW_BUILD_PATH || die 
	wget $BUSYBOX_SOURCE -P $FW_BUILD_PATH || die 
	git clone $OPENSBI_REPO $FW_BUILD_PATH/opensbi || die 

    echo "Unpacking..."
	tar -xvpf $FW_BUILD_PATH/linux-$LINUX_VERSION.tar.xz -C $FW_BUILD_PATH || die 
	tar -xvpf $FW_BUILD_PATH/busybox-$BUSYBOX_VERSION.tar.bz2 -C $FW_BUILD_PATH || die 
    echo "Successfully unpacked!"

    echo "Copying config files!"
	cp $LINUX_CONFIG $FW_BUILD_PATH/linux-$LINUX_VERSION/.config || die 
	cp $BUSYBOX_CONFIG $FW_BUILD_PATH/busybox-$BUSYBOX_VERSION/.config || die
	echo "Copying finished!"

	echo "Building Linux"
	make -C $FW_BUILD_PATH/linux-$LINUX_VERSION ARCH=riscv CROSS_COMPILE=$CROSS_COMPILER -j4 || die 

	echo "Building Busybox"
	make -C $FW_BUILD_PATH/busybox-$BUSYBOX_VERSION CROSS_COMPILE=$CROSS_COMPILER -j4 || die 
	make -C $FW_BUILD_PATH/busybox-$BUSYBOX_VERSION CROSS_COMPILE=$CROSS_COMPILER install || die 

	echo "Building OpenSBI"
	make -C $FW_BUILD_PATH/opensbi CROSS_COMPILE=$CROSS_COMPILER FW_PAYLOAD_PATH=../linux-$LINUX_VERSION/arch/riscv/boot/Image PLATFORM=yariscv -j6 || die 

	echo "Finished!"
}

function build_initramfs()
{
	rm -rf $FW_BUILD_PATH/initramfs || die

	mkdir -p $FW_BUILD_PATH/initramfs || die 
	mkdir -p $FW_BUILD_PATH/initramfs/{bin,dev,etc,lib,mnt,proc,sbin,sys,tmp,var} || die 
	
	cp -R $FW_BUILD_PATH/busybox-$BUSYBOX_VERSION/_install/* $FW_BUILD_PATH/initramfs || die
    
	echo -ne "$INIT_SCRIPT" >> $FW_BUILD_PATH/initramfs/init || die
	chmod +x $FW_BUILD_PATH/initramfs/init

	pushd $FW_BUILD_PATH/initramfs
	find . | cpio -ov --format=newc | gzip -9 >$FW_BUILD_PATH/initramfz.cpio.gz || die 
	popd
	echo "Ready to boot!"
}

function copy_fw_initramfs()
{
    cp -R $FW_BUILD_PATH/initramfz.cpio.gz $FW_BUILD_PATH/opensbi/build/platform/yariscv/firmware/fw_payload.bin $FW_BUILD_PATH/../ || die
    echo "Copied to $(realpath $FW_BUILD_PATH/../)"
}

build_sources
build_initramfs 
copy_fw_initramfs
echo "Done!"

