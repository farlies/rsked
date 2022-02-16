#!/bin/bash
# script invoked by web server when a new schedule is available.
# It will test the schedule, install it, and signal rsked to load it.
#

# These might not be set if invoked via sudo from another account...
export USER=$(/usr/bin/id -un)
export HOME="/home/$USER"
umask 022

RSKED="$HOME/bin/rsked"

DIFF=/usr/bin/diff

RM=/bin/rm
#RM=/usr/bin/rm

MKTEMP=/bin/mktemp
#MKTEMP=/usr/bin/mktemp

PKILL=/usr/bin/pkill

GREP=/bin/grep
#GREP=/usr/bin/grep

MV=/bin/mv
#MV=/usr/bin/mv

CHMOD=/bin/chmod
#CHMOD=/usr/bin/chmod

# could be /tmp instead
LOGDIR="$HOME/logs"

TARGET="$HOME/.config/rsked/schedule.json"

UPLOAD=/var/www/html/upload/newschedule.json

# Any local overrides for the above parameters will be loaded from
# file localparams.sh.  Examples include utility paths above, with
# defaults for Raspbian and commented-out alternatives being for
# Ubuntu and similar.
#
LOCALS=/var/www/misc-bin/localparams.sh

if [ -r ${LOCALS} ]; then
    source ${LOCALS}
fi

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
tsked=$(${MKTEMP} -p /tmp tstrsked.XXXXXX) || exit 1
cp $UPLOAD  $tsked  || exit 2

# Nothing to do if it is identical to running schedule--normal exit
if [ -e "$TARGET" ] ; then
    if ${DIFF} -q $TARGET $tsked &> /dev/null ; then
        echo "Schedules are identical--bypass installation"
        ${RM} -f $tsked
        exit 0
    fi
fi

tlog=$(${MKTEMP} -p $LOGDIR tstrskedlog.XXXXXX) || exit 1

# Run test
if $RSKED --test --schedule $tsked &> $tlog ; then
    # Pass: install, making numbered backup of existing schedule.json
    ${MV} --backup=numbered $tsked "$TARGET"
    # It must be made world readable so web server can grab it
    ${CHMOD} ugo+r "$TARGET"
    echo "Valid schedule"
    # Signal rsked HUP to reload, and cleanup
    ${PKILL} --signal HUP --euid $USER --oldest --exact rsked
    ${RM} -f $tlog
    exit 0
fi

# Failed--emit error lines and cleanup

##rm -f $tsked
${GREP} error $tlog

##rm -f $tlog
exit 2

