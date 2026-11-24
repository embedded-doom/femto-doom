#!/bin/sh
# usage: check.sh HOST WADS DEMOS TRACES [HOST OPTIONS]
# Plays every demo in DEMOS with the host and compares its trace with Chocolate Doom's in
# TRACES. A difference is shown at the first tic it appears in.
set -eu
host=$1
wads=$2
demos=$3
traces=$4
shift 4
got=$(mktemp -d)
trap 'rm -rf "$got"' EXIT
status=0

for lmp in "$demos"/*.lmp; do
	name=$(basename "$lmp" .lmp)
	"$host" "$wads/GRAPHICS.WAD" "$wads/MAPS.WAD" -playdemo "$lmp" -trace "$got/$name.trace" "$@" \
		>"$got/$name.out" 2>&1 || true
	if cmp -s "$got/$name.trace" "$traces/$name.trace"; then
		echo "ok   $name $(tail -n 1 "$got/$name.out")"
	else
		line=$(cmp "$got/$name.trace" "$traces/$name.trace" 2>&1 | sed -n 's/.* line \([0-9]*\).*/\1/p')
		line=${line:-1}
		from=$((line > 1 ? line - 1 : 1))
		echo "FAIL $name, line $line:"
		echo "  femto-doom:     $(sed -n "${from},${line}p" "$got/$name.trace" | tr '\n' ' ')"
		echo "  Chocolate Doom: $(sed -n "${from},${line}p" "$traces/$name.trace" | tr '\n' ' ')"
		tail -n 3 "$got/$name.out" | sed 's/^/  /'
		status=1
	fi
done
exit $status
