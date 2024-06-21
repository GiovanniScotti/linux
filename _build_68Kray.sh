#!/bin/bash

###########################################################
# Builds Linux for 68Kray computer.
###########################################################

set -e

CCCPREFIX="m68k-nommu-uclinux-uclibc-"

SCRIPTDIR=$(dirname "$(realpath $0)")
OUTPUTDIR="${SCRIPTDIR}/68Kray"
TARGETBIN="${OUTPUTDIR}/linux.bin"

GREEN='\033[0;32m'
NOCOL='\033[0m' # No Color

cd $SCRIPTDIR

echo -e "${GREEN}> Building Linux for 68Kray computer...${NOCOL}"
make ARCH=m68k CROSS_COMPILE=${CCCPREFIX} -j6

echo -e "${GREEN}> Building Linux binary file...${NOCOL}"
${CCCPREFIX}objdump -D vmlinux > ${OUTPUTDIR}/vmlinux.dism
cp vmlinux vmlinux.tmp
${CCCPREFIX}strip vmlinux.tmp
${CCCPREFIX}objcopy -O binary vmlinux.tmp ${TARGETBIN}
rm vmlinux.tmp

echo -e "${GREEN}> DONE${NOCOL}"
