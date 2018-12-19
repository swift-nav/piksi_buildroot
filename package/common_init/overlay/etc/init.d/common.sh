# shellcheck disable=SC1091,SC2169,SC2039

get_pid()
{
  local pid_file=$1; shift
  cat "$pid_file" 2>/dev/null || sudo_get_pid "$pid_file"
}

sudo_get_pid()
{
  local pid_file=$1; shift
  sudo cat "$pid_file" 2>/dev/null
}

is_running()
{
  local pid_file=$1; shift
  [ -f "$pid_file" ] && [ -d "/proc/$(get_pid "$pid_file")" ] > /dev/null 2>&1
}

_validate_not_empty()
{
  [[ -n "$(eval echo '${'"$1"':-}')" ]] || {
    echo "Error: the '$1' variable must not be empty" >&2
    return 1
  }
}

configure_dir_resource()
{
  local user=$1; shift
  local path=$1; shift
  local perm=$1; shift

  _validate_not_empty user || return 1
  _validate_not_empty path || return 1
  _validate_not_empty perm || return 1

  configure_dir_resource2 "$user" "$user" "$path" "$perm"
}

configure_dir_resource2()
{
  local user=$1 ; shift
  local group=$1; shift
  local path=$1 ; shift
  local perm=$1 ; shift

  _validate_not_empty user  || return 1
  _validate_not_empty group || return 1
  _validate_not_empty path  || return 1
  _validate_not_empty perm  || return 1

  mkdir -p "$path"
  chown "$user:$group" "$path"
  chmod "$perm" "$path"
}

configure_dir_resource_rec()
{
  local user=$1; shift
  local path=$1; shift
  local dir_perm=$1; shift
  local file_perm=$1; shift

  _validate_not_empty user      || return 1
  _validate_not_empty path      || return 1
  _validate_not_empty dir_perm  || return 1
  _validate_not_empty file_perm || return 1

  configure_dir_resource2 "$user" "$user" "$path" "$dir_perm"

  find "$path" -type d -exec chmod "$dir_perm" {} \;
  find "$path" -type f -exec chmod "$file_perm" {} \;

  find "$path" -exec chown "$user:$user" {} \;
}

configure_file_resource()
{
  local user=$1; shift
  local path=$1; shift
  local perm=$1; shift

  _validate_not_empty user || return 1
  _validate_not_empty path || return 1
  _validate_not_empty perm || return 1

  configure_file_resource2 "$user" "$user" "$path" "$perm"
}

configure_file_resource2()
{
  local user=$1 ; shift
  local group=$1; shift
  local path=$1 ; shift
  local perm=$1 ; shift

  _validate_not_empty user  || return 1
  _validate_not_empty group || return 1
  _validate_not_empty path  || return 1
  _validate_not_empty perm  || return 1

  touch "$path"
  chown "$user:$group" "$path"
  chmod "$perm" "$path"
}

configure_logrotate_file()
{
  local conf_tag=$1 ; shift # must be unique!
  local rotate_file=$1 ; shift

  _validate_not_empty conf_tag     || return 1
  _validate_not_empty rotate_file  || return 1

  path="/etc/logrotate.d/$conf_tag"
  [[ ! -e "$path" ]] || return 1

  touch "$path"

  {
    echo "$rotate_file {"     ;
    echo "      su root root" ;
    echo "      size 5k"      ;
    echo "      copytruncate" ;
    echo "      rotate 5"     ;
    echo "      missingok"    ;
    echo "}"

  } >"$path"
}

_setup_permissions()
{
  if type setup_permissions | grep -q "shell function"; then
    setup_permissions
  fi
}

has_user()
{
  id -u "$1" &>/dev/null
}

has_group()
{
  grep -q "$1" /etc/group
}

add_service_user()
{
  local user=$1; shift

  add_service_user2 "$user" "$user"
}

add_service_user2()
{
  local user=$1; shift
  local group=$1; shift

  has_group "$user" || addgroup -S "$group"
  has_user "$user" || adduser -S -D -H -G "$group" "$user"
}

_release_lockdown=/etc/release_lockdown

lockdown()
{
    [[ -f "$_release_lockdown" ]]
}

detect_piksi_ins()
{
  ins_output_mode=$(query_config --section ins --key output_mode)
  [[ "$ins_output_mode" == "Loosely Coupled" ]]
}
