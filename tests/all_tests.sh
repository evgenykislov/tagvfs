#!/bin/bash

function PrintHelp() {
  echo "--help show help"
  echo "--memleak turn on memory leak detection (linux core with debug_fs is needed)"
}


for arg in "$@"
do
  if [[ "${arg}" == "--help" ]]; then
    PrintHelp
    exit 0
  fi
  if [[ "${arg}" == "--memleak" ]]; then
    TEST_MEMLEAK=1
    export TEST_MEMLEAK
  fi
done


if [[ ${TEST_MEMLEAK+x} ]]; then
  echo clear | sudo tee /sys/kernel/debug/kmemleak
fi

. ./clear.sh
. ./compile_run.sh

. ./test_base_functions.sh
. ./test_copy.sh
. ./test_same_filename_copy.sh

. ./stop.sh

if [[ ${TEST_MEMLEAK+x} ]]; then
  echo scan | sudo tee /sys/kernel/debug/kmemleak
  sudo cat /sys/kernel/debug/kmemleak > memleak.txt
fi
