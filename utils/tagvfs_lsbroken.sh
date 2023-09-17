#!/bin/bash

# Removes files with the tag from tag file system

rootdir=""
logfile="/tmp/tagvfs_rm_log.txt"
finddir=""
target=""

function printhelp {
  indent="  "
  echo "tagvfs_lsbroken.sh lists links with broken (absent) target file"
  echo "Usage:"
  echo "${indent}tagvfs_lsbroken.sh fs-dir"
  echo "${indent}${indent}fs-dir - root directory of tag file system (at root there are tags, only-files folders)"
}


function print_broken {
# $1 target files full path. It's existed
  if [[ "$1" == "" ]]; then
    return
  fi

  local tpath=$(readlink -f "$1")
  if [[ -e "${tpath}" ]] ; then
    return
  fi

  echo "$1"
}

# -----------------------
# main
if [[ $# -lt 1 ]]; then
  printhelp
  exit 1
fi

# Detects store-dir
rootdir=$(realpath $1)
if ! [[ -d "${rootdir}" ]]; then
  echo "Root directory (${rootdir}) of tag filesystem is wrong"
  exit 1
fi

# Enumerate all files and create links
files=`find "${rootdir}/only-files" -maxdepth 1 -type l`
if [[ $? -ne 0 ]]; then
  echo "Can't find files in tag directory. Nothing to do"
  exit 0
fi

while read -r fpath; do
  print_broken "${fpath}"
done <<< "$files"
