#!/bin/bash


# ------------------
# ------------------
if [[ ${TESTDIR} == "" ]]; then
  echo "ERROR: WRONG CONTEXT"
  exit 1
fi

set -o errexit

echo -----
echo "TEST: long names"

SCRIPT_PATH=$(pwd)
pushd ${TESTDIR} > /dev/null
TEST_PATH=$(pwd)
BASEFILE_PATH="${TEST_PATH}/long_names.tag"
ROOT_PATH="${TEST_PATH}/long_names"

# Clear all artifacts
if [[ -f "long_names.tag" ]]; then
  rm -f base_functions.tag
fi
if [[ -d "long_names" ]]; then
  rm -dfr base_functions
fi
mkdir long_names


sudo mount -t tagvfs ${BASEFILE_PATH} ${ROOT_PATH}/
# Path to tag directory
TAG_DIR=${ROOT_PATH}/tags

# Long tag name
STR10="0123456789"
STR20="${STR10}${STR10}"
STR50="${STR20}${STR20}${STR10}"
STR100="${STR50}${STR50}"
STR200="${STR100}${STR100}"

PRELONG_NAME="p${STR200}${STR50}"
LONG_NAME="l${STR200}${STR50}${STR10}"

mkdir "${TAG_DIR}/${PRELONG_NAME}"
set +o errexit
# If filesystem rejects extralong tag name (and returns error) - it's normal/suitable behaviour
mkdir "${TAG_DIR}/${LONG_NAME}" > /dev/null 2> /dev/null
set -o errexit

EXIST_PRELONG_TAG="0"
EXIST_LONG_TAG="0"
for f in ${TAG_DIR}/* ; do
  if [[ "$(basename $f)" == "${PRELONG_NAME}" ]]; then
    EXIST_PRELONG_TAG="1"
  fi
  if [[ "$(basename $f)" == "${LONG_NAME}" ]]; then
    EXIST_LONG_TAG="1"
  fi
done

if ! [[ "${EXIST_PRELONG_TAG}" == "1" ]]; then
  echo "ERROR: LONG NAMES: Can't create prelong name tag correctly"
  exit 1
fi

if [[ "${EXIST_LONG_TAG}" == "1" ]]; then
  echo "ERROR: LONG NAMES: Wrong creation of long name tag. Long tag name should be cut during saving or hasn't created"
  exit 1
fi


# stop, start, repeat checks
. ${SCRIPT_PATH}/comm_wait_umount.sh ${ROOT_PATH}/
sudo mount -t tagvfs ${BASEFILE_PATH} ${ROOT_PATH}/

EXIST_PRELONG_TAG="0"
EXIST_LONG_TAG="0"
for f in ${TAG_DIR}/* ; do
  if [[ "$(basename $f)" == "${PRELONG_NAME}" ]]; then
    EXIST_PRELONG_TAG="1"
  fi
  if [[ "$(basename $f)" == "${LONG_NAME}" ]]; then
    EXIST_LONG_TAG="1"
  fi
done

if ! [[ "${EXIST_PRELONG_TAG}" == "1" ]]; then
  echo "ERROR: LONG NAMES: Can't create prelong name tag correctly"
  exit 1
fi

if [[ "${EXIST_LONG_TAG}" == "1" ]]; then
  echo "ERROR: LONG NAMES: Wrong creation of long name tag"
  exit 1
fi


. ${SCRIPT_PATH}/comm_wait_umount.sh ${ROOT_PATH}/

echo --- OK: LONG_NAMES ---

popd > /dev/null

