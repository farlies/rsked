#!/bin/bash

# Script that takes the new schedule in upload/newschedule.json and
# validates it with rsked.  Will return non-zero if this fails for any
# reason. If the schedule is rejected, a grep of the log for 'error'
# lines will be emitted and there will be a nonzero exit status.
#
# This script accepts no arguments.
# A peer script file, localparams.sh, will be sourced if it exists
# and may redefine variables for the site as needed, e.g. TUSER,
# or utilities that are stored elswhere on target distro.

TESTRSKED=../misc-bin/testrsked.sh
TMPDIR=/tmp
TUSER=pi

SUDO=/usr/bin/sudo

RM=/bin/rm
#RM=/usr/bin/rm

MKTEMP=/bin/mktemp
#MKTEMP=/usr/bin/mktemp

# Create a tmp file for any output from installation...
lout=$($MKTEMP -p $TMPDIR EiEiO.XXXXXX) || exit 1


if [ -r ../misc-bin/localparams.sh ]; then
    source ../misc-bin/localparams.sh
fi

if [ ! -x $TESTRSKED ]; then
    echo "Error: cannot find testrsked"
    exit 1
fi

if ${SUDO} -u $TUSER $TESTRSKED &>$lout ; then
    echo "Valid schedule"
    ${RM} $lout
    exit 0
else
    cat $lout     # TODO: emit the error lines only
    ${RM} $lout
    exit 2
fi
