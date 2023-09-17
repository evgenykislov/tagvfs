#!/bin/bash

# Removes files with the tag from tag file system

rootdir=""
logfile="/tmp/tagvfs_rm_log.txt"
finddir=""
target=""

function printhelp {
  indent="  "
  echo "tagvfs_rm.sh remove file links from filesystem with the tag and target is located in the folder"
  echo "Usage:"
  echo "${indent}tagvfs_rm.sh fs-dir [tag=<ftag>] [target=<ftarget>]"
  echo "${indent}${indent}fs-dir - root directory of tag file system (at root there are tags, only-files folders)"
  echo "${indent}${indent}ftag - the tag has have files"
  echo "${indent}${indent}ftarget - the target folder has have files"
  echo "Example:"
  echo "${indent}tagvfs_rm.sh /tagvfs_dir tag=\"some tag\""
  echo "${indent}${indent}Removes links of files that have got tag 'some tag'"
  echo "${indent}tagvfs_rm.sh /tagvfs_dir target=\"/movies/bad movies/*\""
  echo "${indent}${indent}Removes links of files that are located (the target path) in folder '/movies/bad movies'"
  echo "${indent}tagvfs_rm.sh /tagvfs_dir tag=thriller target=/shorts/*"
  echo "${indent}${indent}Removes links of files that are located in folder /shorts and have got tag 'thriller'. Both conditions are true"
}


function rmfile {
# $1 target files full path. It's existed
  if [[ "$1" == "" ]]; then
    return
  fi

  if ! [[ "${target}" == "" ]]; then
    local tpath=$(dirname "$(readlink -f "${1}")")
    if ! [[ "${tpath}" == "${target}" ]] ; then
      return
    fi
  fi

  local fname=$(basename "$1")
  local fpath="${rootdir}/only-files/${fname}"

  rm -f "${fpath}"
  echo "${fname}"
}

# -----------------------
# main
if [[ $# -lt 1 ]]; then
  printhelp
  exit 1
fi

if [[ $# -lt 2 ]]; then
  echo "Nothing to remove"
  exit 0
fi

# Detects store-dir
rootdir=$(realpath $1)
if ! [[ -d "${rootdir}" ]]; then
  echo "Root directory (${rootdir}) of tag filesystem is wrong"
  exit 1
fi

shift

finddir="${rootdir}/tags"
while [[ -n "$1" ]]; do
  if [[ "${1:0:4}" == "tag=" ]]; then
    finddir="${rootdir}/tags/${1:4}"
  fi
  if [[ "${1:0:7}" == "target=" ]]; then
    target="${1:7}"
  fi

  shift
done

# Enumerate all files and create links
files=`find "${finddir}" -maxdepth 1 -type l`
if [[ $? -ne 0 ]]; then
  echo "Can't find files in tag directory. Nothing to do"
  exit 0
fi

while read -r fpath; do
  rmfile "${fpath}"
done <<< "$files"
