#!/bin/bash

# Wait for mounted point becomes free and umount it
# Waits upto 10 seconds
# $1 - mounted point for umounting

set +o errexit

COUNTER=0
SUCCESS=0
while [[ $COUNTER -lt 100 ]]; do
  ((++COUNTER))
  sleep 0.1
  res=$(lsof $1)
  if [[ $? -eq 1 ]]; then
    # Nothing to output and returns error. It's success
    SUCCESS=1
    break
  elif [[ $? -eq 0 ]]; then
    continue
  elif [[ $res ]]; then
    continue
  else
    # Nothing to output without errors. Is't success too
    SUCCESS=1
    break
  fi
done

if [[ $SUCCESS -eq 0 ]]; then
  echo "ERROR waiting for umount (resources is busy)"
  exit 1
fi

sudo umount $1
if [[ $? != 0 ]]; then
  echo "ERROR umounting"
  exit 1
fi
