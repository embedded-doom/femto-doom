#!/bin/sh
# usage: check.sh <dir with GRAPHICS.WAD and MAPS.WAD>
# Frame hashes of the default build on WADs femtowad built from doom1.wad with its defaults.
wads=$1
status=0

run() {
	expect=$1
	shift
	got=$(./host "$wads/GRAPHICS.WAD" "$wads/MAPS.WAD" "$@" | tail -n 1 | cut -d' ' -f4)
	if [ "$got" = "$expect" ]; then
		echo "ok   $*"
	else
		echo "FAIL $*: $got, expected $expect"
		status=1
	fi
}

run e5916fa6299e5047 -frames 1320
run 05741283216505b9 -frames 1320 -skill 1
run 538aa0ccb500e69c -demo -frames 3000
exit $status
