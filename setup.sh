#! /usr/bin/env bash

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
#    setup.sh -arf     # armv7l FINAL release
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

 -f
    mark this as an official release for packaging

 -g
    use the gcc compiler

- n
    include the NRSC5 radio player

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
FINAL=false
NRSC5=false

while getopts ":acdfngrRx" opt ; do
    case $opt in
        a ) TMACH=armv7l ;;
        c ) sCC=clang; sCXX=clang++ ;;
        d ) BTYPE=debug ;;
        f ) FINAL=true ;;
        g ) sCC=gcc; sCXX=g++ ;;
        n ) NRSC5=true ;;
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

# canonical place in most distros for jsoncpp
JSONCPP=/usr/include/jsoncpp
if [[ -e "$JSONCPP/json/json.h" ]]; then
    echo "jsoncpp include directory is: $JSONCPP"
else
    # Nix, e.g. will add it to CFLAG search path
    echo "jsoncpp include directory TBD by environment"
    JSONCPP=''
fi

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
      -Dfinal=$FINAL \
      -Dwith_nrsc5=$NRSC5 \
      -Djsoncpp_inc="$JSONCPP" \
      -Dstrip=$STRIP  $BUILDDIR

