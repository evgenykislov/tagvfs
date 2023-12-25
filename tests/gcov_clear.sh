#!/bin/bash

set -o errexit

sudo test -d /sys/kernel/debug/gcov
if ! [[ $? -eq 0 ]]; then
  echo "ERROR: There isn't debugfs"
  exit 1
fi

echo clear | sudo tee /sys/kernel/debug/gcov/reset
