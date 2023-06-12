#!/bin/bash
err() { echo 1>&2 "$*"; }
die() { err "%%%%%% FATAL ERROR: $*"; exit 1; }
run() { "$@" || die "command failed with rc=$?"; }

DIR="/data/local/tmp/adbplay"

run ./build.sh
run adb exec-out mkdir -p "$DIR"
run adb push ./libs/armeabi-v7a/adbplay "$DIR"
run adb push test.wav "$DIR"
run adb exec-out "$DIR/adbplay" "file:///$DIR/test.wav"
