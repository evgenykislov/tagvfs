#!/bin/bash

# Removes files with the tag from tag file system

rootdir=""
logfile="/tmp/tagvfs_rm_log.txt"


function printhelp {
  indent="  "
  echo "tagvfs_rm.sh remove files with the tag from the tag file system"
  echo "Usage:"
  echo "${indent}tagvfs_rm.sh fs-dir files-tag"
  echo "${indent}${indent}fs-dir - root directory of tag file system (at root there are tags, only-files folders)"
  echo "${indent}${indent}files-tag - tag of files for removing"
}


function rmfile {
# $1 target files full path. It's existed
  if [[ "$1" == "" ]]; then
    return
  fi

  local fname=$(basename "$1")
  local fpath="${rootdir}/only-files/${fname}"

  rm -f "${fpath}"
}

# -----------------------
# main
if [ $# -lt 1 ] ; then
  printhelp
  exit 1
fi

# Detects store-dir
rootdir=$(realpath $1)
if ! [[ -d "${rootdir}" ]]; then
  echo "Root directory (${rootdir}) of tag filesystem is wrong"
  exit 1
fi

if [[ "$2" == "" ]]; then
  echo "The tag is empty string. Nothing to do"
  exit 0
fi

# Enumerate all files and create links
files=`find "${rootdir}/tags/$2" -maxdepth 1 -type l`
if [[ $? -ne 0 ]]; then
  echo "Can't find files in tag directory. Nothing to do"
  exit 0
fi

while read -r fpath; do
  rmfile "${fpath}"
done <<< "$files"
