#!/bin/bash

. ./common.sh

pushd ${TESTDIR}

sudo umount copy

popd

if [[ -d "${TESTDIR}" ]]; then
  rm -frd ${TESTDIR}
fi
