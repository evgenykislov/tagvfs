#!/bin/bash

if [[ ${TESTDIR} == "" ]]; then
  echo ERROR: WRONG CONTEXT
  exit 1
fi

echo -----
echo "COPY: same filename copy test"

pushd ${TESTDIR} > /dev/null

TPATH=$(pwd)
RPATH="${TPATH}/same_filename_copy"
DPATH="${TPATH}/same_filename_copy_data"

mkdir "${DPATH}"
mkdir "${DPATH}/1"
mkdir "${DPATH}/2"

touch "${DPATH}/1/same_filename_copy test"
touch "${DPATH}/2/same_filename_copy test"

mkdir "${RPATH}"

sudo mount -t tagvfs "${TPATH}/same_filename_copy.tag" "${RPATH}/"

TAGDIR="${RPATH}/tags/new"
mkdir "${TAGDIR}"

ln -s --target-directory="${TAGDIR}" "${DPATH}/1/same_filename_copy test"
# ln -s --target-directory="${TAGDIR}" "${DPATH}/2/same_filename_copy test"
rm "${RPATH}/only-files/same_filename_copy test"
ln -s --target-directory="${TAGDIR}" "${DPATH}/1/same_filename_copy test"

if ! [[ -L "${RPATH}/only-files/same_filename_copy test" ]]; then
  echo "ERROR: SAME FILENAME COPY: the file doesn't exist in only-files folder"
  exit 1
fi

echo "--- OK: SAME FILENAME COPY ---"

sudo umount "${RPATH}"

popd > /dev/null
