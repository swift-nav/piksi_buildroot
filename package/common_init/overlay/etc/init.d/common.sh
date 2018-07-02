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
  [ -f "$pid_file" ] && [ -d "/proc/$(get_pid $pid_file)" ] > /dev/null 2>&1
}

configure_dir_resource()
{
  local user=$1; shift
  local path=$1; shift
  local perm=$1; shift

  mkdir -p $path
  chown $user:$user $path
  chmod $perm $path
}

configure_file_resource()
{
  local user=$1; shift
  local path=$1; shift
  local perm=$1; shift

  touch $path
  chown $user:$user $path
  chmod $perm $path
}

_setup_permissions()
{
  if type setup_permissions | grep -q "shell function"; then
    setup_permissions
  fi
}

has_user()
{
  id -u $1 &>/dev/null;
}

add_service_user()
{
  local user=$1; shift

  has_user $user || addgroup -S $user
  has_user $user || adduser -S -D -H -G $user $user
}

_release_lockdown=/etc/release_lockdown

lockdown()
{
    [[ -f "$_release_lockdown" ]]
}
