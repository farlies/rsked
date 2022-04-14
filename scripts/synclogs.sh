#!/bin/bash
# Synchronize logs and rsked config to the remote host named in arg-1.

if [ $# != 1 ]; then
   echo "Usage: $0  remote-hostname"
   exit 2
fi

/sbin/ifconfig -a >"$HOME/logs/ifconfig"

RHOST=$1
UNIT=`hostname`
rsync_retry.sh "$HOME/logs"     logger@$RHOST:~/$UNIT
rsync_retry.sh "$HOME/logs_old" logger@$RHOST:~/$UNIT
rsync_retry.sh  $HOME/.config/rsked/*.{json,conf}  logger@$RHOST:~/$UNIT/conf
