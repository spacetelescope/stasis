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

pushd() {
    command pushd "$@" 1>/dev/null
}

popd() {
    command popd 1>/dev/null
}

WS_DEFAULT="workspaces/rt_workspace_"
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
    
    export INSTALL_DIR="$WORKSPACE"/local
    if ! mkdir -p "$INSTALL_DIR"; then
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
    if ! cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" -DCMAKE_BUILD_TYPE=Debug "${TOPDIR}"/../..; then
        echo "cmake failed" >&2
        return 1
    fi

    if ! make install; then
        echo "make failed" >&2
        return 1
    fi

    export PATH="$INSTALL_DIR/bin:$PATH"
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


STASIS_TEST_RESULT_FAIL=0
STASIS_TEST_RESULT_PASS=0
STASIS_TEST_RESULT_SKIP=0
run_command() {
    local logfile="$(mktemp).log"
    local cmd="${@}"
    local lines_on_error=100
    /bin/echo "Testing: $cmd "

    $cmd &>"$logfile"
    code=$?
    if (( code )); then
        if (( code == 127 )); then
            echo "... SKIP"
            (( STASIS_TEST_RESULT_SKIP++ ))
        else
            echo "... FAIL"
            if [[ -s "$logfile" ]]; then
                echo "#"
                echo "# Last $lines_on_error line(s) follow:"
                echo "#"
                tail -n $lines_on_error "$logfile"
            fi
            (( STASIS_TEST_RESULT_FAIL++ ))
        fi
    else
        echo "... PASS"
        (( STASIS_TEST_RESULT_PASS++ ))
    fi
    rm -f "$logfile"
}

run_summary() {
    local total=$(( STASIS_TEST_RESULT_PASS + STASIS_TEST_RESULT_FAIL + STASIS_TEST_RESULT_SKIP))
    echo
    echo "[RT] ${STASIS_TEST_RESULT_PASS} tests passed, ${STASIS_TEST_RESULT_FAIL} failed, ${STASIS_TEST_RESULT_SKIP} skipped out of ${total}"
    echo
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
    local retcode=0
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

    if (( retcode )); then
        return 1
    else
        return 0
    fi
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
    if (( retcode )); then
        return 1
    else
        return 0
    fi
}

assert_eq() {
    local a="$1"
    local b="$2"
    local msg="$3"
    if [[ "$a" == "$b" ]]; then
        return 0
    else
        [[ -n "$msg" ]] && echo "'$a' != '$b' :: $msg" >&2
        return 1
    fi
}

assert_file_contains() {
    local file="$1"
    local str="$2"
    local msg="$3"
    if grep -E "$str" "$file" &>/dev/null; then
        return 0
    else
        [[ -n "$msg" ]] && echo "'$str' not in file '$file' :: $msg" >&2
        return 1
    fi
}

clean_up_docker() {
    CONTAINERS_DIR="$WORKSPACE/.local/share/containers"
    # HOME points to the WORKSPACE. The only reason we'd have this directory is if docker/podman was executed
    # Fair to assume if the directory exists, docker/podman is functional.
    if [ -d "$CONTAINERS_DIR" ]; then
        docker run --rm -it -v $CONTAINERS_DIR:/data alpine sh -c '/bin/rm -r -f /data/*'
    fi
}

clean_up() {
    if ([ -z "$RT_KEEP_WORKSPACE" ] || [ -z "$KEEP_WORKSPACE" ]) && [ -d "$WORKSPACE" ]; then
        clean_up_docker
        rm -rf "$WORKSPACE"
    fi

    run_summary
    if (( STASIS_TEST_RESULT_FAIL )); then
        exit 1
    else
        exit 0
    fi
}
