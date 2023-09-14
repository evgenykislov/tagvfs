#!/bin/bash

# Adds symbolic links of all files

MAXDEPTH=20
filter="*"
newtag="new"
rootdir=""
logfile="/tmp/tagvfs_grab_log.txt"
skipdotfiles="y"


function printhelp {
  indent="  "
  echo grab.sh add symbol links of files to specific storage
  echo Usage:
  echo "${indent}grab.sh fs-dir files-dir [new-tag [filter]]"
  echo "${indent}${indent}fs-dir - root directory of tag file system (at root there is tags, only-files folders)"
  echo "${indent}${indent}files-dir - root of directory with files, targets of links"
  echo "${indent}${indent}new-tag - tag for newly created links"
  echo "${indent}${indent}filter - simple name filer"
}


function makelink {
# $1 target files full path. It's existed
  local fname=$(basename "$1")
  local checkpath="${rootdir}/only-files/${fname}"
  local tagdir="${rootdir}/tags/${newtag}/"

  if ! [[ ${skipdotfiles} == "" ]]; then
    if [[ $1 == *"/."* ]]; then
      return
    fi
  fi

  if ! [[ (-e "${checkpath}") || (-L "${checkpath}") ]] ; then
    # file isn't exist
    ln -s --target-directory="${tagdir}" "$1"
    if [[ $? -eq 0 ]]; then
      return
    fi
    echo "ERROR: Can't create link for $1" >> "${logfile}"
    return
  fi

  if [ -L "$checkpath" ] ; then
    # file exist and its sysmbol link
    local target=$(readlink -f "${checkpath}")
    if [ "$1" == "${target}" ] ; then
      # same target. nothing doing
      return
    fi
    echo "WARNING: File for $1 exists with another target - ${target}" >> "${logfile}"
    return
  fi
}

# -----------------------
# main
if [ $# -lt 2 ] ; then
  printhelp
  exit 1
fi

if [ $# -ge 3 ] ; then
  newtag=$3
fi

if [ $# -ge 4 ] ; then
  filter=$4
fi

# Detects store-dir
rootdir=$(realpath $1)
if ! [[ -d "${rootdir}" ]]; then
  echo "Root directory (${rootdir}) of tag filesystem is wrong"
  exit 1
fi

tagdir="${rootdir}/tags/${newtag}"
if ! [[ -d "${tagdir}" ]]; then
  mkdir "${tagdir}"
  if [[ $? -ne 0 ]]; then
    echo "ERROR: Can't create tag folder ${tagdir}"
    exit 1
  fi
fi

filesdir=$(realpath $2)
if ! [[ -d "${filesdir}" ]]; then
  echo "Files directory (${filesdir}) for grabbing is wrong"
  exit 1
fi

# Enumerate all files and create links
files=`find "${filesdir}" -maxdepth ${MAXDEPTH} -type f -name "${filter}"`
if [[ $? -ne 0 ]]; then
  echo "Can't find files in grab directory"
  exit 1
fi

while read -r fpath; do
  makelink "${fpath}"
done <<< "$files"
