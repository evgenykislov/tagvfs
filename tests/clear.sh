#!/bin/bash

. ./common.sh

if [[ -d "${TESTDIR}" ]]; then
  rm -frd ${TESTDIR}
fi
