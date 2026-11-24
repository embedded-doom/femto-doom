#!/bin/sh
# usage: chocolate.sh CHOCOLATE-DOOM DOOM1.WAD DEMOS OUT
# Traces every demo in DEMOS with Chocolate Doom built with chocolate.patch, as OUT/NAME.trace.
set -eu
doom=$1
iwad=$2
demos=$3
out=$4
mkdir -p "$out"

for lmp in "$demos"/*.lmp; do
	name=$(basename "$lmp" .lmp)
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy CHOCO_TRACE="$out/$name.trace" \
		"$doom" -iwad "$iwad" -timedemo "$lmp" -nodraw -nosound -nomusic -nogui >/dev/null 2>&1 || true
	[ -s "$out/$name.trace" ] || { echo "no trace for $name"; exit 1; }
done
echo "$(ls "$out" | wc -l) traces"
