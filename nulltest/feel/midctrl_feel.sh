#!/bin/bash
# run from build/meters with MC=../MeterCapture_artefacts/Release/MeterCapture.exe NT=../../nulltest (see nulltest/feel/README)
# Ctrl pressed or released half way through a drag, a fresh session per mode: midctrl_feel.sh <tag> <vst3> <name> <design> <x,y,dx,dy> ...
# EXTRA=hostwin=1: the editor inside a plain host window that keeps the keyboard focus (as FL)
tag=$1; vst=$2; name=$3; design=$4; shift 4
ctl=("$@")
: > mfeel_$tag.txt
for m in plain ctrl midctrl ctrlup; do
    mm=$m; [ $m = plain ] && mm=""
    D=""; for c in "${ctl[@]}"; do IFS=, read x y dx dy <<< "$c"; n=$(( (${dx#-} + ${dy#-}) / 10 )); D="$D drag=$x,$y,$dx,$dy,$mm,$n"; done
    timeout 300 "$MC" "$vst" "$name" x.raw design=$design $EXTRA $D > mf_$tag.log 2>&1
    echo "-- $m" >> mfeel_$tag.txt; py -3.15 -I "$NT"/parse_drags_any.py mf_$tag.log >> mfeel_$tag.txt
done
