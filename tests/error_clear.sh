#!/bin/bash

. ./common.sh

pushd ${TESTDIR}

sudo umount base_functions
sudo umount copy
sudo umount same_filename_copy
sudo umount long_names

popd

if [[ -d "${TESTDIR}" ]]; then
  rm -frd ${TESTDIR}
fi
