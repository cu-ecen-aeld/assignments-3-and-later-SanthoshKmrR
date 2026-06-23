#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v6.18 ##v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION} linux-stable
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    
    #check this is needed or not, because clean is not required every time
    # make distclean or make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" mrproper

    
    #actually aarch64/configs has only defconfig file, so trying that now
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig #choose which defconfig suitable for aarch64, for now planning to use rpi4
    
    #make menuconfig #enable virtual io's as per videos and enable VPH and debug for generating vmlinux to use in QEMU
    
    #compile and generate kernel and libs
    make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} 

    # This we will do later stage to install in created folders after staging and busybox
    # make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} INSTALL_MOD_PATH=${OUTDIR}/rootfs modules_install
fi

echo "Adding the Image in outdir"
echo " Files in Kernel Build $PWD"
ls ${OUTDIR}/linux-stable/arch/${ARCH}/boot/
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "creating necessary base directories"
mkdir -p ${OUTDIR}/rootfs && cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log
cd -

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://git.busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    # configure busybox in rootfs folder
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} menuconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=../rootfs/ ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "***** Library dependencies *****"
${CROSS_COMPILE}readelf -a ../rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ../rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "*** CC_FOLDER ****"
CC_FOLDER=$(${CROSS_COMPILE}gcc -print-sysroot) # find crosscompiler folder location

echo "*** COPY to CC_FOLDER ****"
cp -a $CC_FOLDER/lib/ld-linux-aarch64.so.1 ../${OUTDIR}/rootfs/lib/
cp -a $CC_FOLDER/lib64/libm.so.6 ../${OUTDIR}/rootfs/lib64/
cp -a $CC_FOLDER/lib64/libresolv.so.2 ../${OUTDIR}/rootfs/lib64/
cp -a $CC_FOLDER/lib64/libc.so.6 ../${OUTDIR}/rootfs/lib64/

# TODO: Make device nodes
echo "*** Make Nodes ****"
sudo mknod -m 666 ../${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ../${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
echo "*** find and rebuild writer **** "
cd ${FINDER_APP_DIR}  #../${OUTDIR}/../finder-app
echo $PWD
echo ${FINDER_APP_DIR}
make clean
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
cd ${OUTDIR}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "*** copy assign2 files to root fs **** "
cp -rf ${OUTDIR}/../finder-app/* ${OUTDIR}/rootfs/home/
rm -rf ${OUTDIR}/rootfs/home/conf 
mkdir ${OUTDIR}/rootfs/home/conf
cp -rf ${OUTDIR}/../conf/* ${OUTDIR}/rootfs/home/conf/

# TODO: Chown the root directory
echo "*** change OWN **** "
sudo chown root:root ${OUTDIR}/rootfs #change owner to root of root folder

# TODO: Create initramfs.cpio.gz
echo "*** create CRAM info ****"
cd ${OUTDIR}/rootfs
echo $PWD
find . | cpio -H newc -ov --owner root:root > ../${OUTDIR}/initramfs.cpio
cd ../${OUTDIR}
gzip -f initramfs.cpio
