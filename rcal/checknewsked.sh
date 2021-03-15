#!/bin/bash
# Script that takes the new schedule in upload/newschedule.json
# and validates it with rsked.  Will return non-zero if this fails
# for any reason. If rsked rejects, a grep of the log for 'error'
# lines will be returned.
#
# This script accepts no arguments.
# TODO: install a good new schedule.
#
RSKED=$HOME/bin/rsked
lout=$(mktemp -p /tmp EiEiO.XXXXXX) || exit 1
nsked=upload/newschedule.json
#
if [[ ! -r $nsked ]]; then
    echo "Missing or unreadable file: $nsked"
    exit 1
fi

if $RSKED --test --schedule=$nsked 2>$lout ; then
    echo "Good schedule"
    rm $lout
    exit 0
else
    grep error $lout
    rm $lout
    exit 2
fi
