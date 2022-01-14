#!/bin/bash

# Script that takes the new schedule in upload/newschedule.json and
# validates it with rsked.  Will return non-zero if this fails for any
# reason. If the schedule is rejected, a grep of the log for 'error'
# lines will be emitted and there will be a nonzero exit status.
#
# This script accepts no arguments.

TESTRSKED=/home/sharp/bin/testrsked

# Create a tmp file for any output from installation...
lout=$(mktemp -p /tmp EiEiO.XXXXXX) || exit 1

if /usr/bin/sudo -u sharp $TESTRSKED &>$lout ; then
    echo "Valid schedule"
    rm $lout
    exit 0
else
    cat $lout     # TODO: emit the error lines only
    rm $lout
    exit 2
fi
