copy_from_sd()
{
  local sd_path=$1
  local filename=$2
  local output_path=$3
  local sd_file="$sd_path/$filename"
  local output_file="$output_path/$filename"

  if [ -f "$sd_file" ]; then
    rm -f "$output_file"
    echo "Copying $filename from SD card"
    mkdir -p "$output_path"
    cp "$sd_file" "$output_file"

    if [ ! -f "$output_file" ]; then
      echo "ERROR: copy failed for $output_file"
    fi
  fi
}

copy_from_net()
{
  local server_ip=$1
  local filename=$2
  local output_path=$3
  local uuid=`cat /factory/uuid`
  local net_file="PK$uuid/$filename"
  local output_file="$output_path/$filename"
  local tmp_file="/tmp/$filename"

  echo "Retrieving $filename from network"
  tftp -g -r "$net_file" -l "$tmp_file" -b 65464 "$server_ip"

  if [ -f "$tmp_file" ]; then
    rm -f "$output_file"
    echo "Copying $filename from network"
    mkdir -p "$output_path"
    cp "$tmp_file" "$output_file"

    if [ ! -f "$output_file" ]; then
      echo "ERROR: copy failed for $output_file"
    fi
  fi
}

