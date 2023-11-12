#!/bin/bash


if $(sudo test -e /sys/kernel/debug/kmemleak); then
  echo clear | sudo tee /sys/kernel/debug/kmemleak
fi

. ./clear.sh
. ./compile_run.sh

. ./test_copy.sh
. ./test_same_filename_copy.sh


. ./stop.sh

if $(sudo test -e /sys/kernel/debug/kmemleak); then
  echo scan | sudo tee /sys/kernel/debug/kmemleak
  sudo cat /sys/kernel/debug/kmemleak > memleak.txt
fi
