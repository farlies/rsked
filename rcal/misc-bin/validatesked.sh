#!/bin/bash
# Script that takes the new schedule in upload/newschedule.json
# and validates it with json schema.  Will return non-zero if this fails
# for any reason.
#
# This script accepts no arguments.

VALIDATOR=/usr/local/bin/yajsv
SCHEMA=./sked_schema-2.0.json
NEWSKED=../upload/newschedule.json
TMPDIR=/tmp

if [[ ! -x $VALIDATOR ]]; then
    echo "Error: unable to run the JSON validator: $VALIDATOR"
    exit 3
fi
if [[ ! -r $SCHEMA ]]; then
    echo "Error: unable to access the JSON schema: $SCHEMA"
    exit 3
fi

if [[ ! -r $NEWSKED ]]; then
    echo "Missing or unreadable file: $NEWSKED"
    exit 1
fi

lout=$(/usr/bin/mktemp -p $TMPDIR EiEiO.XXXXXX) || exit 1

if $VALIDATOR -s $SCHEMA $NEWSKED >$lout ; then
    echo "Valid schedule"
    /usr/bin/rm $lout
    exit 0
else
    cat $lout
    #/usr/bin/rm $lout
    exit 2
fi
