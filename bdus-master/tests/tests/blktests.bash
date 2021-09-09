#!/usr/bin/env bash
# ---------------------------------------------------------------------------- #

set -ex

readonly backends=( ioctl mmap rw )

# get script directory

readonly script_dir="$( readlink -e "$0" | xargs dirname )"

# compile driver

readonly ram_driver_binary="$(mktemp)"
readonly loop_driver_binary="$(mktemp)"

readonly temp_dir="$(mktemp -d)"

trap '{
    rm -r "${ram_driver_binary}" "${loop_driver_binary}" "${temp_dir}";
    }' EXIT

cc -O2 "${script_dir}/../drivers/ram.c" -lbdus -o "${ram_driver_binary}"
cc -O2 "${script_dir}/../drivers/loop.c" -lbdus -o "${loop_driver_binary}"

# get blktests

cd "${temp_dir}"

wget --no-cache --quiet https://github.com/osandov/blktests/archive/master.zip
unzip -q master.zip
cd blktests-master
make

# run tests

for backend in "${backends[@]}"; do

    ram_device_path="$("${ram_driver_binary}")"

    trap '{
        bdus destroy "${ram_device_path}";
        rm -r "${ram_driver_binary}" "${loop_driver_binary}" "${temp_dir}";
        }' EXIT

    loop_device_path="$("${loop_driver_binary}" "${backend}" "${ram_device_path}")"

    trap '{
        bdus destroy "${loop_device_path}";
        bdus destroy "${ram_device_path}";
        rm -r "${ram_driver_binary}" "${loop_driver_binary}" "${temp_dir}";
        }' EXIT

    TEST_DEVS="${loop_device_path}" DEVICE_ONLY=1 EXCLUDE=block/008 ./check

    bdus destroy "${loop_device_path}"

    trap '{
        bdus destroy "${ram_device_path}";
        rm -r "${ram_driver_binary}" "${loop_driver_binary}" "${temp_dir}";
        }' EXIT

    bdus destroy "${ram_device_path}"

    trap '{
        rm -r "${ram_driver_binary}" "${loop_driver_binary}" "${temp_dir}";
        }' EXIT

done

# check if any kbdus assertion failed

! dmesg | grep -q 'kbdus:.*assertion' || { dmesg; false; }

# ---------------------------------------------------------------------------- #
