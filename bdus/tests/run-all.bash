#!/usr/bin/env bash
# ---------------------------------------------------------------------------- #

set -ex

# get script directory

readonly script_dir="$( readlink -e "$0" | xargs dirname )"

# check usage

if (( $# != 0 )); then
    >&2 echo "Usage: ${0}"
    exit 2
fi

# run all tests

cd "${script_dir}/tests"

for test in *; do
    "${test}"
done

# ---------------------------------------------------------------------------- #
