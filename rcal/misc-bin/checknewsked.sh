#!/bin/bash

# Script that takes the new schedule in upload/newschedule.json and
# validates it with rsked.  Will return non-zero if this fails for any
# reason. If the schedule is rejected, a grep of the log for 'error'
# lines will be emitted and there will be a nonzero exit status.
#
# This script accepts no arguments.
# A peer script file, localparams.sh, will be sourced if it exists
# and may redefine variables for the site as needed, e.g. TUSER.

TESTRSKED=../misc-bin/testrsked.sh
TMPDIR=/tmp
TUSER=pi

# Create a tmp file for any output from installation...
lout=$(/usr/bin/mktemp -p $TMPDIR EiEiO.XXXXXX) || exit 1


if [ -r ../misc-bin/localparams.sh ]; then
    source ../misc-bin/localparams.sh
fi

if [ ! -x $TESTRSKED ]; then
    echo "Error: cannot find testrsked"
    exit 1
fi

if /usr/bin/sudo -u $TUSER $TESTRSKED &>$lout ; then
    echo "Valid schedule"
    /usr/bin/rm $lout
    exit 0
else
    cat $lout     # TODO: emit the error lines only
    /usr/bin/rm $lout
    exit 2
fi
