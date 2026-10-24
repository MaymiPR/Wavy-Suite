#!/bin/bash
# run from build/meters with MC=../MeterCapture_artefacts/Release/MeterCapture.exe NT=../../nulltest (see nulltest/feel/README)
# finds fader thumbs: scan_thumb.sh <tag> <vst3> <name> <design> <x,ylo,yhi,axis> ...   (axis v: scan y at x; h: scan x at y=ylo..)
tag=$1; vst=$2; name=$3; design=$4; shift 4
D=""
for c in "$@"; do
    IFS=, read a lo hi ax <<< "$c"
    for p in $(seq $lo 5 $hi); do
        if [ $ax = v ]; then D="$D drag=$a,$p,0,-3,,3 drag=$a,$((p - 3)),0,3,,3"; else D="$D drag=$p,$a,3,0,,3 drag=$((p + 3)),$a,-3,0,,3"; fi
    done
done
timeout 1500 "$MC" "$vst" "$name" x.raw design=$design $D > scan_$tag.log 2>&1
py -3.15 -I "$NT"/parse_drags_any.py scan_$tag.log | grep -v "no change" > scan_$tag.txt
