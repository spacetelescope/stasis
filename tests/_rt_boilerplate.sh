#!/usr/bin/env bash
here="$(dirname ${BASH_SOURCE[0]})"
source $here/setup.sh

TEST_NAME=

#test_fails() {
#    return 1
#}
#
#test_passes() {
#    return 0
#}
#
#test_skips() {
#    return 127
#}

setup_workspace "$TEST_NAME"

#run_command test_fails
#run_command test_passes
#run_command test_skips
#run_command false  # fails
#run_command true   # passes

teardown_workspace "$TEST_NAME"