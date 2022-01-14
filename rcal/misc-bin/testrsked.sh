#!/bin/bash
# script invoked by web server when a new schedule is available.
# It will test the schedule, install it, and signal rsked to load it.
#

# These might not be set if invoked via sudo from another account...
export USER=$(/usr/bin/id -un)
export HOME="/home/$USER"
umask 022

RSKED="$HOME/bin/rsked"

# could be /tmp instead
LOGDIR="$HOME/logs"

TARGET="$HOME/.config/rsked/schedule.json"

UPLOAD=/var/www/html/upload/newschedule.json

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
if [ ! -x $RSKED ] ; then
    echo "Rsked not executable by $UID"
    exit 1
fi

if [ ! -f $UPLOAD ] ; then
    echo "New schedule seems to be missing?"
    exit 1
fi

# Copy the new schedule away from uploads (since it might change
# there unexpectedly)...
tsked=$(mktemp -p /tmp tstrsked.XXXXXX) || exit 1
cp $UPLOAD  $tsked  || exit 2

# Nothing to do if it is identical to running schedule--normal exit
if [ -e "$TARGET" ] ; then
    if /usr/bin/diff -q $TARGET $tsked &> /dev/null ; then
        echo "Schedules are identical--bypass installation"
        /usr/bin/rm -f $tsked
        exit 0
    fi
fi

tlog=$(mktemp -p $LOGDIR tstrskedlog.XXXXXX) || exit 1

# Run test
if $RSKED --test --schedule $tsked &> $tlog ; then
    # Pass: install, making numbered backup of existing schedule.json
    /usr/bin/mv --backup=numbered $tsked "$TARGET"
    # It must be made world readable so web server can grab it
    /usr/bin/chmod ugo+r "$TARGET"
    echo "Valid schedule"
    # Signal rsked HUP to reload, and cleanup
    /usr/bin/pkill --signal HUP --euid $USER --oldest --exact rsked
    /usr/bin/rm -f $tlog
    exit 0
fi

# Failed--emit error lines and cleanup

##rm -f $tsked
/usr/bin/grep error $tlog

##rm -f $tlog
exit 2

