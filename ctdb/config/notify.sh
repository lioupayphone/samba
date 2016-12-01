#!/bin/sh

# This script is activated by setting CTDB_NOTIFY_SCRIPT=/etc/ctdb/notify.sh
# in /etc/sysconfig/ctdb

# This is script is invoked from ctdb when certain events happen.  See
# /etc/ctdb/notify.d/README for more details.

d=$(dirname $0)
nd="${d}/notify.d"

ok=true

echo "begin to execute notify.sh"

if [ ! -f "/var/run/ctdb/synclock" ] ; then
	touch /var/run/ctdb/synclock > /dev/null 2>&1

fi

for i in "${nd}/"* ; do
    # Don't run files matching basename
    case "${i##*/}" in
	*~|*,|*.rpm*|*.swp|README) continue ;;
    esac

    # Files must be executable
    [ -x "$i" ] || continue

    # Flag failures
    if [ "x$2" != "x" ] ; then
	flock -x -w 5 /var/run/ctdb/synclock -c "\"$i\" \"$1\" \"$2\"" || ok=false
	 continue
    fi
     
    "$i" "$1" || ok=false
done

$ok
