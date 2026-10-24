#!/bin/bash
# run from build/meters with MC=../MeterCapture_artefacts/Release/MeterCapture.exe NT=../../nulltest (see nulltest/feel/README)
# Ctrl drag rates by mouse step size, a fresh session per step size: ctrl_feel.sh <tag> <vst3> <name> <design> <x,y,sx,sy> ...
# (sx, sy: the drag direction, -1 / 0 / 1); 200 px per drag
tag=$1; vst=$2; name=$3; design=$4; shift 4
ctl=("$@")
: > cfeel_$tag.txt
for step in 1 2 3 10; do
    n=$((200 / step)); D=""
    for c in "${ctl[@]}"; do IFS=, read x y sx sy <<< "$c"; D="$D drag=$x,$y,$((sx * 200)),$((sy * 200)),ctrl,$n"; done
    timeout 400 "$MC" "$vst" "$name" x.raw design=$design $D > cf_$tag.log 2>&1
    echo "-- ctrl, $step px steps" >> cfeel_$tag.txt
    py -3.15 -I "$NT"/parse_drags_any.py cf_$tag.log >> cfeel_$tag.txt
done
