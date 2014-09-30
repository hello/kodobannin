#!/bin/sh
set -ex
# make sure build_list exists
builds=./tools/build_list.txt
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
    if [ -n "$p" ]; then
        build_targ $p
    fi
done < $builds
