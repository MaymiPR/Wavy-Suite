#!/bin/bash
# run from build/meters with MC=../MeterCapture_artefacts/Release/MeterCapture.exe NT=../../nulltest (see nulltest/feel/README)
# fader drag rates from the default thumbs, one fresh session per mode: fader_feel.sh <tag> <vst3> <name> <design> <x,y,dx,dy> ...
tag=$1; vst=$2; name=$3; design=$4; shift 4
ctl=("$@")
: > ffeel_$tag.txt
for mode in "plain:50" "plain:5" "ctrl:50" "ctrl:5"; do
    m=${mode%%:*}; st=${mode##*:}; mm=$m; [ $m = plain ] && mm=""
    D=""
    for c in "${ctl[@]}"; do IFS=, read x y dx dy <<< "$c"; D="$D drag=$x,$y,$dx,$dy,$mm,$st"; done
    timeout 300 "$MC" "$vst" "$name" x.raw design=$design $D > ff_$tag.log 2>&1
    echo "-- $m, steps $st" >> ffeel_$tag.txt
    py -3.15 -I "$NT"/parse_drags_any.py ff_$tag.log >> ffeel_$tag.txt
done
