#!/bin/bash

. ./common.sh

pushd ${TESTDIR}

sudo umount copy
sudo umount same_filename_copy

popd

if [[ -d "${TESTDIR}" ]]; then
  rm -frd ${TESTDIR}
fi
