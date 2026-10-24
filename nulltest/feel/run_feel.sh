#!/bin/bash
# run from build/meters with MC=../MeterCapture_artefacts/Release/MeterCapture.exe NT=../../nulltest (see nulltest/feel/README)
# runs the knob-feel script on a plug-in: run_feel.sh <tag> <vst3> <name> <design width> <x,y,axis> ...   (axis v / h)
tag=$1; vst=$2; name=$3; design=$4; shift 4
D=""
for c in "$@"; do
    IFS=, read x y ax <<< "$c"
    for i in 0 1 2 3; do
        mod=""; [ $i -ge 2 ] && mod=ctrl; steps=50; [ $((i % 2)) -eq 1 ] && steps=5
        s=$([ $((i % 2)) -eq 0 ] && echo -50 || echo 50)
        if [ $ax = v ]; then D="$D drag=$x,$y,0,$s,$mod,$steps"; else D="$D drag=$x,$y,$((-s)),0,$mod,$steps"; fi
    done
    D="$D drag=$x,$y,3,0,wheel drag=$x,$y,-3,0,wheel drag=$x,$y,3,0,wheelctrl drag=$x,$y,-3,0,wheelctrl"
done
timeout 900 "$MC" "$vst" "$name" x.raw design=$design $D > feel_$tag.log 2>&1
py -3.15 -I "$NT"/parse_drags_any.py feel_$tag.log > feel_$tag.txt
