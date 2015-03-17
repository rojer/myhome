#!/bin/bash

trap 'kill -HUP 0' EXIT

src_file="$1"
dst_spec="$2"
nc_port=88
bs=2097152

if ! [[ $dst_spec =~ ^([^:]+):(.+) ]]; then
  echo bad dst spec $dst_spec >&2
  exit 1
fi

dst_host_spec=${BASH_REMATCH[1]}
dst_host=${dst_host_spec/*@/}
dst=${BASH_REMATCH[2]}
dst_src_base="$dst/$(basename $src_file)"
dst_dir=$(dirname "$dst")
dst_dir_src_base="$dst_dir/$(basename $src_file)"

i=0
IFS=
r=`ssh "$dst_host_spec" "for f in \"$dst_src_base\" \"$dst\" \"$dst_dir\"; do /bin/ls -l -d \"\\\$f\" 2>&1; done"`
[[ $? != 0 ]] && exit $?
#r=$(cat q)
IFS=''
while read -r line; do
  i=$((i + 1))
  # Parse result of a successful ls, pulling out file type and size.
  [[ $line =~ ^(.)[^[:blank:]]+[[:blank:]]+[[:digit:]]+[[:blank:]]+[^[:blank:]]+[[:blank:]]+[^[:blank:]]+[[:blank:]]+([[:digit:]]+) ]];
  ftype=${BASH_REMATCH[1]}
  fsize=${BASH_REMATCH[2]}
  echo x $line
  echo y $i \"$ftype\" \"$fsize\"
  if [[ $i == 1 && $ftype != "" ]]; then
    # dst + basename(src_base) exists => destination is a directory
    if [[ $ftype == '-' ]]; then
      dst_exists=1
      dst_file=$dst_src_base
      dst_size=$fsize
      break
    else
      echo "$dst_src_base already exists and is not a file." >&2
      exit 1
    fi
  elif [[ $i == 2 && $ftype != "" ]]; then
    # dst exists, but not dst + bn(src) (that's i = 1).
    if [[ $ftype == '-' ]]; then
      # Destination is a file, fair and square.
      dst_exists=1
      dst_file=$dst
      dst_size=$fsize
      break
    elif [[ $ftype == 'd' ]]; then
      # Destination is a directory, add basename(src) to it.
      dst_exists=0
      dst_file=$dst_src_base
      dst_size=0
      break
    else
      echo "$dst_dir already exists and is neither a file or a directory." >&2
      exit 1
    fi
  elif [[ $i == 3 ]]; then
    # dst_dir exists
    if [[ $ftype == '-' ]]; then
      dst_exists=1
      dst_file=$dst_src_base
      dst_size=$fsize
      break
    elif [[ $ftype == "" ]]; then
      echo "Destination directory ($dst_dir) does not exist." >&2
      exit 1
    else
      echo "$dst_dir already exists and is neither a file or a directory." >&2
      exit 1
    fi
  fi
done <<< "$r"

pos=$(($dst_size / $bs))
echo $dst_exists $dst_size $pos $dst_file

exec 3< <(ssh "$dst_host_spec" " \
  done=0; \
  trap 'kill 0' EXIT; \
  trap 'done=1' SIGUSR1; \
  exec 3< <(nc -l $nc_port && kill -USR1 \$\$ || kill -HUP \$\$); \
  exec 4< <(dd bs=$bs seek=$pos of=\"$dst_file\" <&3 && kill -USR1 \$\$ || kill -HUP \$\$); \
  sleep 1 && echo ok; \
  while [[ \$done != 1 ]]; do sleep 1; done" 2>&1)

#while read <&3 line; do
#  echo "$line"
#done
#exit 0

read <&3 line
if [[ "$line" != "ok" ]]; then
  echo "$line" >&2
  exit 1
fi

echo 'good'
dd bs=$bs skip=$pos if="$src_file" | nc "$dst_host" $nc_port
echo "done $?"
read <&3 line
echo "$line"
