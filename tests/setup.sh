#!/usr/bin/env bash
set -o pipefail

export LOGFILE_STASIS="stasis.log"
export LOGFILE_INDEXER="stasis_indexer.log"
export CHECK_OUTPUT_PATTERNS=()

unset STASIS_SYSCONFDIR
if [ -n "$GITHUB_TOKEN" ] && [ -z "$STASIS_GH_TOKEN"]; then
    export STASIS_GH_TOKEN="$GITHUB_TOKEN"
else
    export STASIS_GH_TOKEN="anonymous"
fi

if [[ -z "$PYTHON_VERSIONS" ]]; then
    PYTHON_VERSIONS=(
        3.10
        3.11
        3.12
    )
fi

setup_script_dir="$(dirname ${BASH_SOURCE[0]})"
export TOPDIR=$(pwd)
export TEST_DATA="$TOPDIR"/data

WS_DEFAULT=rt_workspace_
setup_workspace() {
    if [ -z "$1" ]; then
        echo "setup_workspace requires a name argument" >&2
        return 1
    fi
    WORKSPACE="${WS_DEFAULT}$1"
    rm -rf "$WORKSPACE"
    if ! mkdir -p "$WORKSPACE"; then
        echo "directory creation failed. cannot continue" >&2
        return 1;
    fi
    WORKSPACE="$(realpath $WORKSPACE)"
    
    export PREFIX="$WORKSPACE"/local
    if ! mkdir -p "$PREFIX"; then
        echo "directory creation failed. cannot continue" >&2
        return 1;
    fi

    export BUILD_DIR="$WORKSPACE"/build
    if ! mkdir -p "$BUILD_DIR"; then
        echo "directory creation failed. cannot continue" >&2
        return 1;
    fi

    pushd "$WORKSPACE"
    export LANG="C"
    export HOME="$WORKSPACE"
    . /etc/profile
}

teardown_workspace() {
    if [ -z "$1" ]; then
        echo "teardown_workspace requires a workspace path" >&2
        return 1
    elif ! [[ ${WS_DEFAULT}$1 =~ ${WS_DEFAULT}.* ]]; then
        echo "$1 is not a valid workspace" >&2
        return 1
    fi
    popd
    clean_up
}

install_stasis() {
    pushd "$BUILD_DIR"
    if ! cmake -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Debug "${TOPDIR}"/../..; then
        echo "cmake failed" >&2
        return 1
    fi

    if ! make install; then
        echo "make failed" >&2
        return 1
    fi

    export PATH="$PREFIX/bin:$PATH"
    hash -r
    if ! type -P stasis; then
        echo "stasis program not on PATH" >&2
        return 1
    fi

    if ! type -P stasis_indexer; then
        echo "stasis_indexer program not on PATH" >&2
        return 1
    fi
    popd
}

run_stasis() {
    local logfile="$LOGFILE_STASIS"
    $(type -P stasis) --unbuffered -v $@ 2>&1 | tee "$logfile"
}

run_stasis_indexer() {
    local logfile="$LOGFILE_INDEXER"
    local root="$1"
    if [ -z "$root" ]; then
        echo "run_stasis_indexer root directory cannot be empty" >&2
        exit 1
    fi
    $(type -P stasis_indexer) --web --unbuffered -v "$root"/* 2>&1 | tee "$logfile"
}

check_output_add() {
    local pattern="$1"
    CHECK_OUTPUT_PATTERNS+=("$pattern")
}

check_output_reset() {
    CHECK_OUTPUT_PATTERNS=()
}

check_output_stasis_dir() {
    local startdir="$1"
    local logfile="$LOGFILE_STASIS"

    echo "#### Files ####"
    find $startdir | sort
    echo

    echo "#### Contents ####"
    files=$(find $startdir -type f \( -name "$logfile" -o -name '*.yml' -o -name '*.md' -o -name '*.stasis' -o -name '*.ini' \) | sort)
    for x in $files; do
        echo
        echo "FILENAME: $x"
        echo
        if [ "$x" == "$logfile" ]; then
            # do not print thousands of lines of output we _just_ sat through
            echo "Output omitted"
        else
            cat "$x"
            echo "[EOF]"
        fi
        echo

        for cond in "${CHECK_OUTPUT_PATTERNS[@]}"; do
            if grep --color -H -n "$cond" "$x" >&2; then
                echo "ERROR DETECTED IN $x!" >&2
                retcode+=1
            fi
        done
    done

    return $retcode
}

check_output_indexed_dir() {
    local retcode=0
    local startdir="$1"
    local logfile="$2"

    echo "#### Files ####"
    find $startdir | sort

    for cond in "${CHECK_OUTPUT_PATTERNS[@]}"; do
        if grep --color -H -n "$cond" "$logfile" >&2; then
            echo "ERROR DETECTED IN INDEX OPERATION!" >&2
            retcode+=1
        fi
    done

    echo "#### Contents ####"
    files=$(find $startdir -type f \( -name '*.html' \) | sort)
    for x in $files; do
        echo
        echo "FILENAME: $x"
        echo
        cat "$x"
        echo "[EOF]"
        echo
    done
    return $retcode
}

clean_up() {
    if [ -z "$RT_KEEP_WORKSPACE" ] && [ -d "$WORKSPACE" ]; then
        rm -rf "$WORKSPACE"
    fi
}