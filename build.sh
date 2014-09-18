set -e
builds=build_list.txt
function sync_repo()
{
    git submodule update && git submodule sync
}
function clean_repo()
{
    make clean
}
function build_targ()
{
    make $1
}

sync_repo && clean_repo
while read p; do
    build_targ $p
done < $builds
