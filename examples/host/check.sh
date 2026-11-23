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

run b74a5fa4c7682d26 -frames 1320
run da1690d86790f666 -frames 1320 -skill 1
run b3e4f65c85c4bead -demo -frames 3000
exit $status
