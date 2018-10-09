#!/usr/bin/env bash

cd "$(dirname "$0")"

KEY_INDEX=1

host_dir=$1; shift
build_dir=$1; shift
target_dir=$1; shift

install_manifest=${build_dir}/install_manifest.txt

if [[ ! -f "$install_manifest" ]]; then
  echo "ERROR: CMake install manifest not found: $install_manifest" >&2
  exit 1
fi

declare -a cleanup_items=()

cleanup_on_exit()
{
  for i in "${cleanup_items[@]}"; do
    rm $i
  done
}

trap 'cleanup_on_exit' EXIT
add_cleanup_item() { cleanup_items+=("$*"); }

enc_file()
{
  local the_file=$1; shift

  echo "Encrypting on target: '$the_file' ..."

  local enc_out=$(mktemp)
  add_cleanup_item "$enc_out"

  ${host_dir}/usr/bin/encrypt_and_sign \
    "${target_dir}/$the_file" \
    "${target_dir}/${the_file}.enc" \
    ${KEY_INDEX} >${enc_out}

  if [[ $? -ne 0 ]]; then

    echo "ERROR: encrypt_and_sign tool failed:" >&2
    cat "${enc_out}" >&2

    exit 1
  fi

  rm -v "${target_dir}/$the_file"
}

enc_wrap_file()
{
  local the_file=$1; shift

  case $the_file in
  */local_pose_playback|*.a)
    echo "Removing unecessary file: ${the_file}..."
    rm -v "${target_dir}/${the_file}"
    ;;
  *.so|*/bin/*|*/lib/*)
    enc_file "$the_file"
    ;;
  *)
    echo "Ignoring: $the_file"
    ;;
  esac
}

while IFS= read file; do
  enc_wrap_file "$file"
done < <(./read_install_manifest.py "$install_manifest")
