#!/bin/bash

# Use this script to initialize common build configurations:
#
# Usage examples:
#    setup.sh         # `arch` debug
#    setyp.sh mybuild # `arch` debug build directory = "mybuild"
#    setup.sh -c      # `arch` debug, force clang compiler
#    setup.sh -g      # `arch` debug, force gcc compiler
#    setup.sh -xr     # x86_64 release
#    setup.sh -xd     # x86_64 debug
#    setup.sh -ar     # armv7l release
#    setup.sh -ad     # armv7l debug

# Part of the rsked project. Copyright 2020 Steven A. Harp


print_usage()
{
cat <<EOF
Usage:  ./setup.sh [-d|-r] [-a|-x] [-c|-g] [builddir]"

This script will invoke meson to prepare a build directory.
Without arguments, will prepare a build directory for a debug
build on the native architecture using gcc.

Options:
 -a
    build for armv7l architecture

 -d
    select a debug build

 -c
    use the clang compiler

 -g
    use the gcc compiler

 -r
    select a release build

 -R
    select a release build but do not strip binaries

 -x
    build for x86_64 architecture

 builddir
    optional name for the build directory
EOF
}

sCC=gcc
sCXX=g++
BTYPE='debug'
TMACH=$(arch)
NOSTRIP=false

while getopts ":acdgrRx" opt ; do
    case $opt in
        a ) TMACH=armv7l ;;
        c ) sCC=clang; sCXX=clang++ ;;
        d ) BTYPE=debug ;;
        g ) sCC=gcc; sCXX=g++ ;;
        r ) BTYPE=release ;;
        R ) BTYPE=release ; NOSTRIP=true ;;
        x ) TMACH=x86_64 ;;
        \? ) print_usage
             exit 1
    esac
done
shift $(($OPTIND - 1))

if [[ $# > 1 ]]; then
    echo "Invalid argument list."
    print_usage
    exit 1
fi

# to strip or not to strip...
#
if [[ $BTYPE = 'release' ]]; then
    if [[ $NOSTRIP = 'false' ]]; then
	STRIP=true
    else
	STRIP=false
    fi
else
    STRIP=false
fi
echo "Strip binaries: $STRIP"

if [[ $# == 1 ]]; then
    BUILDDIR=$1
else
    BUILDDIR="${BTYPE}-${TMACH}"
fi

if [[ -d $BUILDDIR ]]; then
    echo "Error: build directory $BUILDDIR already exists"
    exit 1
fi

echo "Setup for target-arch=$TMACH, build-type=$BTYPE in: $BUILDDIR"
echo "Compiler: $sCXX / $sCC"

#-----------------------------

CC=$sCC CXX=$sCXX \
meson setup --buildtype $BTYPE \
      -Dprefix="$HOME" \
      -Dtarget_machine=$TMACH \
      -Ddatadir=.config \
      -Dstrip=$STRIP  $BUILDDIR

