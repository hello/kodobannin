#!/bin/sh
set -e
# make sure build_list exists
builds=build_list.txt
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
