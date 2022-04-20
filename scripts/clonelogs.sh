#!/bin/bash
# Use rclone to synchronize logs and rsked config to rclone destination
# e.g. AWS, Azure, Box, Dropbox
#
# Takes 1 argument: the name of the *preconfigured* rclone remote,
# Note: no trailing colon on argument!
# e.g.     clonelogs.sh db.rsked

# you might need to adjust the location of the latest rclone binary:
RCLONE=/usr/local/bin/rclone


if [ $# != 1 ]; then
   echo "Usage: $0  [rclone_remote]"
   exit 2
fi

/sbin/ifconfig -a >"$HOME/logs/ifconfig"

REMOTE=$1
UNIT=`hostname`

$RCLONE sync $HOME/logs ${REMOTE}:${UNIT}/logs
$RCLONE sync $HOME/logs_old ${REMOTE}:${UNIT}/logs_old
$RCLONE copy --max-depth 0 $HOME/.config/rsked ${REMOTE}:${UNIT}/conf
