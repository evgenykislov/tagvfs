#!/bin/bash


function CheckTag1Exist() {
  EXIST_TAG1="0"
  for f in ${TAGDIR}/* ; do
    if [[ "$(basename $f)" == "tag1" ]]; then
      EXIST_TAG1="1"
    fi
  done

  if ! [[ "${EXIST_TAG1}" == "1" ]]; then
    echo "ERROR: BASE FUNCTIONS: Tag tag1 is created but it hasn't listed in directory"
    exit 1
  fi

  EXIST_TAG1="0"
  for f in ${RPATH}/only-tags/* ; do
    if [[ $(basename $f) == "tag1" ]]; then
      EXIST_TAG1="1"
    fi
  done

  if ! [[ "${EXIST_TAG1}" == "1" ]]; then
    echo "ERROR: BASE FUNCTIONS: Tag tag1 is created but it hasn't listed in only-tags directory"
    exit 1
  fi
}


function CheckBF1Exist() {
  if ! [[ -L "${TAGDIR}/base_functions_001" ]]; then
    echo "ERROR: BASE FUNCTIONS: file base_functions_001 doesn't exist"
    exit 1
  fi

  if ! [[ "$(readlink -f "${TAGDIR}/base_functions_001")" == "${TPATH}/base_functions_001" ]] ; then
    echo "ERROR: BASE FUNCTIONS: Wrong target for base_functions_001"
    exit 1
  fi

  if ! [[ -L "${TAGDIR}/no-tag1/base_functions_001" ]]; then
    echo "ERROR: BASE FUNCTIONS: file no-tag1/base_functions_001 doesn't exist"
    exit 1
  fi

  if ! [[ "$(readlink -f "${TAGDIR}/no-tag1/base_functions_001")" == "${TPATH}/base_functions_001" ]] ; then
    echo "ERROR: BASE FUNCTIONS: Wrong target for no-tag1/base_functions_001"
    exit 1
  fi
}


# ------------------
# ------------------
if [[ ${TESTDIR} == "" ]]; then
  echo "ERROR: WRONG CONTEXT"
  exit 1
fi

set -o errexit

echo -----
echo "TEST: base functions test"

pushd ${TESTDIR} > /dev/null

TPATH=$(pwd)
# Root path of mount point
RPATH="${TPATH}/base_functions"

# Clear all artifacts
if [[ -f "base_functions.tag" ]]; then
  rm -f base_functions.tag
fi
if [[ -d "base_functions" ]]; then
  rm -dfr base_functions
fi
mkdir base_functions


sudo mount -t tagvfs $(pwd)/base_functions.tag $(pwd)/base_functions/
# Path to tag directory
TAGDIR=${RPATH}/tags

# Check tag creation
# -----
mkdir "${TAGDIR}/tag1"

set +o errexit
if [[ $(mkdir ${TAGDIR}/tag1 > /dev/null 2> /dev/null) ]]; then
  echo "ERROR: BASE FUNCTIONS: double creewtion of the same tag"
  exit 1
fi
set -o errexit

CheckTag1Exist


# Check filelink creation and exstance into no-tags
# -----
touch base_functions_001
ln -s --target-directory="${TAGDIR}" "${TPATH}/base_functions_001"

set +o errexit
if [[ $(ln -s --target-directory="${TAGDIR}" "${TPATH}/base_functions_001" > /dev/null 2> /dev/null) ]]; then
  echo "ERROR: BASE FUNCTIONS: double creation of the same symbol link"
  exit 1
fi
set -o errexit

CheckBF1Exist

# stop, start, repeat checks
sudo umount $(pwd)/base_functions/
sudo mount -t tagvfs $(pwd)/base_functions.tag $(pwd)/base_functions/
CheckTag1Exist
CheckBF1Exist

echo "--- OK: BASE FUNCTIONS ---"

sudo umount $(pwd)/base_functions/

popd > /dev/null
