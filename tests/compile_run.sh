#!/bin/bash

. ./common.sh

pushd "${BUILDDIR}"
# make clean
make
popd

if ! [[ -d "${TESTDIR}" ]] ; then
  mkdir "${TESTDIR}"
fi

cp "${BUILDDIR}/tagvfs.ko" "${TESTDIR}"

if [[ $(lsmod | grep tagvfs) ]]; then
  sudo rmmod tagvfs
fi

sudo insmod ./${TESTDIR}/tagvfs.ko

# Get debug information

BASEPATH=$(pwd)
pushd /sys/module/tagvfs/sections
echo ".text .data .bss" > ${BASEPATH}/sections.txt
sudo cat .text .data .bss >> ${BASEPATH}/sections.txt
popd

objdump -DS ./${TESTDIR}/tagvfs.ko > disasm.txt
