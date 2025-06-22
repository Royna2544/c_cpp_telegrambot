#!/bin/bash 

not_in_array() {
  local value="$1"
  shift
  local arr=("$@")
  for item in "${arr[@]}"; do
    if [[ "$item" == "$value" ]]; then
      return 1  # Found
    fi
  done
  return 0  # Not found
}

if [ $# != 1 ]; then
    echo "Usage: $0 [socket schema version]";
    exit 1;
fi

known_versions=("1")
if not_in_array "$1" known_versions; then
   echo "Unknown schema version: $1";
#   exit 1;
fi

if [ ! -d .git ]; then
    echo "Run this in project root";
    exit 1;
fi

set -x
for fbs in src/socket/schemas/$1/*.fbs; do
    python resources/scripts/gen_example_from_fbs.py $fbs > ${fbs%.fbs}.json;
done
