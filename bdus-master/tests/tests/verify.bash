#!/usr/bin/env bash
# ---------------------------------------------------------------------------- #

set -ex

readonly backends=( ioctl mmap rw )

# get script directory

readonly script_dir="$( readlink -e "$0" | xargs dirname )"

# compile drivers

readonly ram_driver_binary="$(mktemp)"
readonly loop_driver_binary="$(mktemp)"

trap '{ rm "${ram_driver_binary}" "${loop_driver_binary}"; }' EXIT

cc -O2 "${script_dir}/../drivers/ram.c" -lbdus -o "${ram_driver_binary}"
cc -O2 "${script_dir}/../drivers/loop.c" -lbdus -o "${loop_driver_binary}"

# create devices

readonly ram_device_path="$("${ram_driver_binary}")"

trap '{
    bdus destroy "${ram_device_path}";
    rm "${ram_driver_binary}" "${loop_driver_binary}";
    }' EXIT

readonly loop_device_path="$("${loop_driver_binary}" "${backends[0]}" "${ram_device_path}")"

trap '{
    bdus destroy --no-flush "${loop_device_path}";
    bdus destroy "${ram_device_path}";
    rm "${ram_driver_binary}" "${loop_driver_binary}";
    }' EXIT

# run workload

(
    set -ex
    while true; do
        for backend in "${backends[@]}"; do
            sleep 2
            if (( RANDOM % 2 )); then
                pkill -SIGKILL -f "${loop_driver_binary}"
                sleep 1
            fi
            "${loop_driver_binary}" "${backend}" "${ram_device_path}" "${loop_device_path}"
        done
    done
) &

time fio - <<EOF &
[global]
filename=${loop_device_path}
numjobs=16
size=64m
offset_increment=64m
io_size=256m
blocksize=512
runtime=5M
group_reporting=1

[trim-write-verify]
readwrite=trimwrite
ioengine=libaio
iodepth=32
direct=1
verify=crc32c

[read]
readwrite=randread
ioengine=libaio
iodepth=32
direct=1
EOF

# wait for swapper or fio to end (fio should end first on success)

wait -n
(( $? == 0 )) # ensure that it was fio that ended

# destroy devices

trap '{
    bdus destroy "${ram_device_path}";
    rm "${ram_driver_binary}" "${loop_driver_binary}";
    }' EXIT

bdus destroy --no-flush "${loop_device_path}"

trap '{ rm "${ram_driver_binary}" "${loop_driver_binary}"; }' EXIT

bdus destroy "${ram_device_path}"

# wait for swapper to terminate

wait || true

# check if any kbdus assertion failed

! dmesg | grep -q 'kbdus:.*assertion' || { dmesg; false; }

# ---------------------------------------------------------------------------- #
