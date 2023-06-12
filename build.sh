#!/bin/bash
err() { echo 1>&2 "$*"; }
die() { err "%%%%%% FATAL ERROR: $*"; exit 1; }
run() { "$@" || die "command failed with rc=$?"; }

BUILDER=ykasidit/android_ndk_c_rust_go_builder:latest

build() { run docker run -u $(id -u) --rm -v $(pwd):/build "$BUILDER" /bin/sh -c "$*"; }

err "Build script user: $(id)"
build 'echo Docker user: $(id) && cd /build && ndk-build clean && ndk-build V=1 -j $(nproc)'

err
err "==== BUILD SUCCESSFUL ===="
err
ls -l libs/*/*
