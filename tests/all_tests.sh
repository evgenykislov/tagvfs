#!/bin/bash


echo clear | sudo tee /sys/kernel/debug/kmemleak

. ./clear.sh
. ./compile_run.sh

. ./test_copy.sh
. ./test_same_filename_copy.sh


. ./stop.sh

echo scan | sudo tee /sys/kernel/debug/kmemleak


sudo cat /sys/kernel/debug/kmemleak > memleak.txt
