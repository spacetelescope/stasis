#!/usr/bin/env bash
set -x
unset STASIS_SYSCONFDIR
if [ -n "$GITHUB_TOKEN" ] && [ -z "$STASIS_GH_TOKEN"]; then
    export STASIS_GH_TOKEN="$GITHUB_TOKEN"
else
    export STASIS_GH_TOKEN="anonymous"
fi

topdir=$(pwd)

ws="rt_workspace"
mkdir -p "$ws"
ws="$(realpath $ws)"

prefix="$ws"/local
mkdir -p "$prefix"

bdir="$ws"/build
mkdir -p "$bdir"

pushd "$bdir"
cmake -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_BUILD_TYPE=Debug "${topdir}"/../..
make install
export PATH="$prefix/bin:$PATH"
popd

pushd "$ws"
    type -P stasis
    type -P stasis_indexer

    stasis --no-docker --no-artifactory --unbuffered -v "$topdir"/generic.ini
    retcode=$?

    set +x

    echo "#### Files ####"
    find stasis/*/output | sort
    echo

    echo "#### Contents ####"
    files=$(find stasis/*/output -type f \( -name '*.yml' -o -name '*.md' -o -name '*.stasis' -o -name '*.ini' \) | sort)
    for x in $files; do
        echo
        echo "FILENAME: $x"
        echo
        cat "$x"
        echo "[EOF]"
        echo

        fail_on_main=(
            "(null)"
        )
        for cond in "${fail_on_main[@]}"; do
            if grep --color -H -n "$cond" "$x" >&2; then
                echo "ERROR DETECTED IN $x!" >&2
                retcode=2
            fi
        done
    done

    # Something above failed, so drop out. Don't bother indexing.
    # Don't clean up either.
    (( retcode )) && exit $retcode

    fail_on_indexer=(
        "(null)"
    )
    logfile="stasis_indexer.log"
    set -x
    stasis_indexer --web --unbuffered -v stasis/* 2>&1 | tee "$logfile"

    set +x
    find output

    for cond in "${fail_on_indexer[@]}"; do
        if grep --color -H -n "$cond" "$logfile" >&2; then
            echo "ERROR DETECTED IN INDEX OPERATION!" >&2
            exit 1
        fi
    done

popd

rm -rf "$ws"

exit $retcode