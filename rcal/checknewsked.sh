#!/bin/bash
# Script that takes the new schedule in upload/newschedule.json
# and validates it with rsked.  Will return non-zero if this fails
# for any reason. If rsked rejects, a grep of the log for 'error'
# lines will be returned.
#
# This script accepts no arguments.
# TODO: install a good new schedule.

#  There are problems running rsked anywhere but installed HOME :-(
#  so instead we will use a JSON validator.
#RSKED=$HOME/bin/rsked
#RSKED=/home/sharp/bin/rsked

VALIDATOR=/usr/local/bin/yajsv
SCHEMA=./sked_schema-2.0.json

if [[ ! -x $VALIDATOR ]]; then
    echo "Error: unable to run the JSON validator"
    exit 3
fi
if [[ ! -r $SCHEMA ]]; then
    echo "Error: unable to access the JSON schema"
    exit 3
fi

lout=$(mktemp -p /tmp EiEiO.XXXXXX) || exit 1
nsked=upload/newschedule.json
#
if [[ ! -r $nsked ]]; then
    echo "Missing or unreadable file: $nsked"
    exit 1
fi

if $VALIDATOR -s $SCHEMA $nsked >$lout ; then
    echo "Valid schedule"
    rm $lout
    exit 0
else
    cat $lout
    #rm $lout
    exit 2
fi
