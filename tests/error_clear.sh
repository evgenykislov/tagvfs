#!/bin/bash

TESTDIR="test"

pushd ${TESTDIR}

sudo umount base_functions
sudo umount copy
sudo umount same_filename_copy
sudo umount long_names
sudo umount only-tags-feature

popd

if [[ -d "${TESTDIR}" ]]; then
  rm -frd ${TESTDIR}
fi
