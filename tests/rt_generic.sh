#!/usr/bin/env bash
here="$(dirname ${BASH_SOURCE[0]})"
source $here/setup.sh

TEST_NAME=generic
PYTHON_VERSIONS=(
    3.11
)

setup_workspace "$TEST_NAME"
run_command install_stasis
for py_version in "${PYTHON_VERSIONS[@]}"; do
    run_command run_stasis --python "$py_version" \
        --no-docker \
        --no-artifactory \
        "$TEST_DATA"/"$TEST_NAME".ini
done

check_output_add "(null)"
run_command check_output_stasis_dir stasis/*/output
check_output_reset

# NOTE: indexer default output directory is "output"
check_output_add "(null)"
run_command run_stasis_indexer stasis
run_command check_output_indexed_dir output
check_output_reset

run_command assert_file_contains "$LOGFILE_STASIS" "USE EXTERNAL" "External packages should have been used"

teardown_workspace "$TEST_NAME"