#!/bin/bash


# set -o errexit

sudo test -d /sys/kernel/debug/gcov
if ! [[ $? -eq 0 ]]; then
  echo "ERROR: There isn't debugfs"
  exit 1
fi

REP_NAME="tagvfs_$(date '+%Y-%m-%d_%H_%M_%S')"
RESULT_DIR=$(pwd)

pushd ../tagvfs > /dev/null
SRC_DIR=$(pwd)
popd > /dev/null

DFS_DIR="/sys/kernel/debug/gcov/${SRC_DIR}"
sudo test -d ${DFS_DIR}
if ! [[ $? -eq 0 ]]; then
  echo "There isn't coverage data for module"
  exit 1
fi

echo collect data ...

pushd ${DFS_DIR}
lcov -t "${REP_NAME}" -o "${RESULT_DIR}/${REP_NAME}.info" -c -d .
popd

pushd "${RESULT_DIR}"
genhtml -o "${REP_NAME}" "${RESULT_DIR}/${REP_NAME}.info"
popd
