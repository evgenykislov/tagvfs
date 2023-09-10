#!/bin/bash

. ./common.sh

set -e

if [[ $(lsmod | grep tagvfs) ]]; then
  sudo rmmod tagvfs
fi

. ./clear.sh
