#/usr/bin/env bash

[[ -z "$DEBUG" ]] || set -x

IFS=

args=("$@")
cmd="$(echo ${args[@]:1})"

exec nix-shell --run "piksi-env ${args[0]} \"$cmd\""
