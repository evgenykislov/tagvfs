#!/bin/bash

# Set common variables
TESTDIR="test"
export TESTDIR
SUMM_RES=0

trap "ExitHandler" EXIT
function ExitHandler() {
${SHELL} ./tagvfs_stop
# Clear playground
if [[ -d "${TESTDIR}" ]]; then
  rm -frd ${TESTDIR}
fi

}


function PrintHelp() {
  echo "--help show help"
  echo "--memleak turn on memory leak detection (linux core with debug_fs is needed)"
}


function RunTest() {
# $1 name of the test
  ${SHELL} ./${1} > ${1}.txt 2> ${1}.txt
  if ! [[ $? -eq 0 ]]; then
    echo "Test $1 FAILED"
    cat ${1}.txt
    SUMM_RES=0
  fi
}


# --------------------
# --------------------
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

mkdir ${TESTDIR}

if [[ ${TEST_MEMLEAK+x} ]]; then
  echo clear | sudo tee /sys/kernel/debug/kmemleak
fi


rm -f *.txt
$(${SHELL} ./tagvfs_compile_run > compile_run.txt 2> compile_run.txt)
if ! [[ $? -eq 0 ]]; then
  echo "ERROR: Can't compile and run tagvfs module"
  cat compile_run.txt
  exit 1
fi

SUMM_RES=1
RunTest test_base_functions
RunTest test_copy
RunTest test_same_filename_copy
RunTest test_long_names
RunTest test_only_tags_feature

if ! [[ ${SUMM_RES} -eq 0 ]]; then
  echo "ALL TESTS executed successfully"
else
  echo "THERE ARE SOME ERRORS !!!"
fi

if [[ ${TEST_MEMLEAK+x} ]]; then
  echo scan | sudo tee /sys/kernel/debug/kmemleak
  sudo cat /sys/kernel/debug/kmemleak > memleak.txt
fi

# Exit trap will clears all
