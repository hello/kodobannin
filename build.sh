set -e
# make sure build_list exists
builds=build_list.txt
function clean_repo()
{
    make clean
}
function build_targ()
{
    make $1
}

clean_repo
while read p; do
    build_targ $p
done < $builds
