#!/usr/bin/env bash
# ---------------------------------------------------------------------------- #

set -ex

# get script directory

readonly script_dir="$( readlink -e "$0" | xargs dirname )"

# compile example drivers

readonly binary="$(mktemp)"
trap '{ rm "${binary}"; }' EXIT

for example in $(find "${script_dir}/../../examples" -name '*.c'); do

    cc \
        -std=c99 \
        -Werror -Wall -Wextra -Wno-unused-parameter -pedantic \
        "${example}" -lbdus -o "${binary}"

done

# ---------------------------------------------------------------------------- #
