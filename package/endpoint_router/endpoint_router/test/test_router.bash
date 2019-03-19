#!/usr/bin/env bash

#set -x

set -euo pipefail
IFS=$'\n\t'

[[ -n "${TEST_DATA_DIR:-}" ]] || { echo "ERROR: TEST_DATA_DIR must not be empty" >&2; exit 1; }
[[ -n "${TARGET_DIR:-}" ]] || { echo "ERROR: TARGET_DIR must not be empty" >&2; exit 1; }

echo '<<<< ' Starting scripted test ...

red=$(echo -ne "\e[91m")
grn=$(echo -ne "\e[36m")
rst=$(echo -ne "\e[0m")

pass_color() {
  echo "${grn}$*${rst}"
}

fail_color() {
  echo "${red}$*${rst}"
}

export PK_METRICS_PATH=/tmp/endpoint_router.metrics

rm -rf $PK_METRICS_PATH
mkdir $PK_METRICS_PATH

export PK_METRICS_PATH=$PK_METRICS_PATH

"$TARGET_DIR"/usr/bin/endpoint_router -f "$TEST_DATA_DIR"/run_test.yml --name test_router &
router_pid=$!

declare -a socat_list
declare -A socat_map

next_fifo_fd=9

setup_socat()
{
  local n=$1

  socat_list+=("$n")

  socat_map[inp_$n]=/tmp/sbp_port_$n.inp
  socat_map[out_$n]=/tmp/sbp_port_$n.out
  socat_map[pub_$n]=/tmp/sbp_port_$n.pub
  socat_map[sub_$n]=/tmp/sbp_port_$n.sub
  socat_map[fd_$n]=$next_fifo_fd

  next_fifo_fd=$(( next_fifo_fd + 1 ))

  local inp="${socat_map[inp_$n]}"
  local out="${socat_map[out_$n]}"
  local fd="${socat_map[fd_$n]}"
  local pub="${socat_map[pub_$n]}"
  local sub="${socat_map[sub_$n]}"
  
  rm -f "$inp" "$out"

  # Bind FD to the fifo so it'll stay open as we write messages
  mkfifo "$inp"
  eval "exec $fd<>$inp"

  socat -u FILE:/dev/stdin UNIX-CONNECT:"$sub",type=5 <"$inp" &
  socat_map[inp_pid_$n]=$!

  socat -u unix-connect:"$pub",type=5 file:/dev/stdout >"$out" &
  socat_map[out_pid_$n]=$!
}

send_message()
{
  local n=$1; shift
  local msg=$1; shift

  local inp="${socat_map[inp_$n]}"

  echo "$msg" >"$inp"
}

output_file()
{
  local n=$1
  echo "${socat_map[out_$n]}"
}

cleanup_one()
{
  local n=$1

  local inp="${socat_map[inp_$n]}"
  local out="${socat_map[out_$n]}"
  local fd="${socat_map[fd_$n]}"
  local pub="${socat_map[pub_$n]}"
  local sub="${socat_map[sub_$n]}"
  local inp_pid="${socat_map[inp_pid_$n]}"
  local out_pid="${socat_map[out_pid_$n]}"

  eval "exec $fd<&-"

  rm $inp $out $pub $sub

  kill $inp_pid $out_pid
  wait $inp_pid $out_pid
}

cleanup_all()
{
  set +e && echo -n '>>>> ' Cleaning up ...

  for n in ${socat_list[*]}; do
    cleanup_one "$n"
  done

  kill $router_pid
  wait $router_pid &>/dev/null

  echo " DONE"
}

trap 'cleanup_all' EXIT

run_test()
{
  local test_func=$1; shift
  local succ_msg=$1; shift
  local fail_msg=$1; shift

  if "$test_func"; then
    echo "$(pass_color PASS): ${succ_msg}"
  else
    echo "$(fail_color FAIL): ${fail_msg}"
    exit 1
  fi
}

main()
{
  ### Firmware socket ###
  setup_socat "firmware"

  ### Firmware FileIO socket ###
  setup_socat "fileio_firmware"

  ### External socket ###
  setup_socat "external"

  ### External socket ###
  setup_socat "fileio_external"

  local file_read=acz
  local file_read_dir=adz
  local other=qqq
  local pad=xxxxxyyyy

  ## Allow things time to setup...
  sleep 0.2

  ### TEST: if firmware sends a FileIO req it should show up on fw_fileio pub socket ###
  test_1() {
    local fw_fileio_msg=${file_read}${pad}001
    send_message "firmware" "$fw_fileio_msg"
    grep -q "$fw_fileio_msg" "$(output_file "fileio_firmware")"
  }

  run_test test_1 \
    "firmware fileio message DID go to fileio daemon" \
    "firmware fileio message DID NOT go to fileio daemon"

  ### TEST: if fileio gets sent to external, it should not show up on the fileio socket ###
  test_2() {
    local ext_fileio_msg=${file_read_dir}${pad}002
    send_message "external" "$ext_fileio_msg"
    grep -q "$ext_fileio_msg" "$(output_file "firmware")" && return 1
    grep -q "$ext_fileio_msg" "$(output_file "fileio_external")" && return 0
  }

  run_test test_2 \
    "external fileio message DID NOT go to firmware" \
    "external fileio message DID go to firmware"


  ### TEST: Other messages should show up as input to the firmware port ###

  test_3() {
    local other_msg=${other}${pad}003
    send_message "external" ${other_msg}
    grep -q "$other_msg" "$(output_file "firmware")"
  }

  run_test test_3 \
    "external 'other' message DID go to firmware port" \
    "external 'other' message DID NOT go to firmware port"

  test_4() {
    local fw_fileio_msg=${file_read_dir}${pad}004
    send_message "firmware" ${fw_fileio_msg}
    grep -q "$fw_fileio_msg" "$(output_file "fileio_firmware")"
  }

  run_test test_4 \
    "firmware fileio message DID go to firmware fileio port" \
    "firmware fileio message DID NOT go to firmware fileio port"
}

main
