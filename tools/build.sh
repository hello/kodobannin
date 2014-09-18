#!/bin/sh
set -ex
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# make sure build_list exists
builds=$DIR/build_list.txt
clean_repo()
{
    make clean
}
build_targ()
{
    make $1
}

#clean_repo
while read p; do
    build_targ $p
done < $builds
