
function sync_repo()
{
    git submodule update && git submodule sync
}
sync_repo && echo "ok"
