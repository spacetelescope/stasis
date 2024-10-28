set -e

here="$(dirname ${BASH_SOURCE[0]})"
source $here/setup.sh

TEST_NAME=generic
PYTHON_VERSIONS=(
    3.11
)
setup_workspace "$TEST_NAME"
install_stasis
for py_version in "${PYTHON_VERSIONS[@]}"; do
    run_stasis --python "$py_version" \
        --no-docker \
        --no-artifactory \
        "$TOPDIR"/"$TEST_NAME".ini
done

check_output_add "(null)"
check_output_stasis_dir stasis/*/output
check_output_reset

# NOTE: indexer default output directory is "output"
check_output_add "(null)"
run_stasis_indexer stasis
check_output_indexed_dir output
check_output_reset

teardown_workspace "$TEST_NAME"
