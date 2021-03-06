#!/bin/bash

# Use this script to create a binary distribution from a build.
# The result is a tgz that may be expanded in the target machine
# HOME directory.  If the option "-n" is given, then no tgz is
# created and the temporary dist directory is preserved (allows
# customization before archiving.)
#
# Usage examples:
#   ./pack_rsked.sh debug x86_64
#   ./pack_rsked.sh -c armv7l  release armv7l
#   ./pack_rsked.sh -n debug x86_64

# Part of the rsked project. Copyright 2020 Steven A. Harp

# exit immediately on error
set -e


print_usage()
{
cat <<EOF
Usage:  ./pack_rsked.sh [-n] [-c confdir] {debug|release} {armv7l|x86_64}

This script will prepare a compressed archive with rsked binaries, sripts
and configuration files.

Options:
 -n 
    do not create the compressed archive or delete the dist directory

 -c confdir
    use configuration from config/confdir instead of config/example-ARCH

EOF
}


# Find a real tar
if [[ -e /usr/bin/tar ]]; then
    TAR=/usr/bin/tar
elif [[ -e /bin/tar ]]; then
    TAR=/bin/tar
else
    echo "Unable to locate a canonical tar command"
    exit 1
fi
       

ALTCONFDIR=
NOCOMPRESS=0

while getopts ":nc:" opt ; do
    case $opt in
        n ) NOCOMPRESS=1 ;;
        c ) ALTCONFDIR=$OPTARG ;;
        \? ) print_usage
             exit 1
    esac
done
shift $(($OPTIND - 1))

if [[ $# != 2 ]]; then
    echo "Invalid argument list."
    print_usage
    exit 1
fi

# Establish builddir and confdir:
builddir="$1-$2"
build_type=$1
if [[ $ALTCONFDIR == "" ]]; then
    confdir="config/example-$2"
else
    confdir="config/$ALTCONFDIR"
fi

if [[ ! -d $builddir ]]; then
    echo "No build directory named './$builddir'"
    exit 1
fi

if [[ ! -d $confdir ]]; then
    echo "No configuration directory named './$confdir'"
    exit 1
fi

vvv=$(grep VERSION_STR $builddir/version.h)
version=${vvv#\#define VERSION_STR \"}
version=${version%\"}
echo "; version $version"


tgtdir=$(mktemp -d dist.XXXX)
echo "; constructing install image in $tgtdir/"

mkdir -p $tgtdir/.config/rsked
mkdir -p $tgtdir/.config/mpd
mkdir -p $tgtdir/.config/gqrx

# populate binaries and scripts
bindir=$tgtdir/bin
mkdir $bindir
cp $builddir/rsked $bindir
cp $builddir/cooling $bindir
cp $builddir/vumonitor $bindir

if [[ "$build_type" == "release" ]]; then
    strip $bindir/cooling
    strip $bindir/rsked
    strip $bindir/vumonitor
    echo "; stripped release binaries"
fi

## GQRX
gqrxbin="../gqrx/build/gqrx"
# Note: assume gqrx binary is already stripped if it needs to be!
if [[ -e $gqrxbin ]] ; then
    cp $gqrxbin $bindir
    strip $bindir/gqrx
    echo "; found gqrx binary, installing"
else
    echo "; did Not find gqrx binary: $gqrxbin"
fi

## NRSC5
nrsc5bin="../nrsc5/build/src/nrsc5"
# Note: assume nrsc5 binary is already stripped if it needs to be!
if [[ -e $nrsc5bin ]] ; then
    cp $nrsc5bin $bindir
    strip $bindir/nrsc5
    echo "; found nrsc5 binary, installing"
else
    echo "; did Not find nrsc5 binary: $nrsc5bin"
fi

SCRIPTS="check_inet.sh btremote.pl btremote.service check_playlist.sh\
  check_deploy.pl gpiopost.py rskrape.pl startup_rsked synclogs.sh"

for scr in $SCRIPTS; do
    cp scripts/$scr $bindir
done

# populate config/rsked
rctarget=$tgtdir/.config/rsked
cp $confdir/rsked/*.json $rctarget/
cp $confdir/rsked/*.pl $rctarget/
cp $confdir/rsked/example-crontab $rctarget/
cp $confdir/rsked/example-ci.conf $rctarget/
cp doc/*  $rctarget/
cp scripts/sked_schema-2.0.json $rctarget/
cp -R resource $rctarget/
mkdir $rctarget/motd
cp motd/each* $rctarget/motd/
cp motd/README $rctarget/motd/

# populate config/mpd
cp -R $confdir/mpd/* $tgtdir/.config/mpd/

# populate config/gqrx
cp -R $confdir/gqrx/* $tgtdir/.config/gqrx/

echo "; image constructed in $tgtdir"
#tree -a $tgtdir

if [[ 1 == $NOCOMPRESS ]] ; then
    exit 0
fi

# create tarball
archname=rsked${version}.tgz
$TAR -c -z -f $archname -C $tgtdir .

echo "; deleting temp directory $tgtdir"
rm -rf $tgtdir

echo "; archive $archname may be expanded in the target machine like:"
echo ";"
echo ";   tar xzf  $archname"
echo ";"
echo "; (note this will clobber existing files with the same names)"
