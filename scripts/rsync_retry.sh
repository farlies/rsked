#!/bin/bash

### BASED ON: See: http://gist.github.com/366269 Runs rsync, retrying
### on errors up to a maximum number of tries.  On failure script
### waits for internect connection to come back up by pinging
### google.com before continuing.
###
### Usage: $ ./rsync-retry.sh source destination
### Example: $ ./rsync-retry.sh user@server.example.com:~/* ~/destination/path/
###
### IMPORTANT:
### To avoid repeated password requests use public key instead of passwords
### "ssh-keygen" (with no password), then "ssh-copy-id user@server.example.com"

# ----------------------------- rSync Options -------------------------------------

OPT="--inplace -vza"
MAX_RETRIES=10
RECURSE="Y"

TURL=https://www.mprnews.org/robots.txt
#TURL=http://www.google.com

# -------------------- ---------------------------------------------------------


COM="rsync $OPT -e 'ssh -o \"ServerAliveInterval 10\"' $@"
echo
echo  "via: $COM"


# Trap interrupts and exit instead of continuing the loop
trap "echo Ctl+C Detected... Exiting!; exit;" SIGINT SIGTERM

COUNT=0
START=$SECONDS

# Set the initial exit value to failure
false
while [ $? -ne 0 -a $COUNT -lt $MAX_RETRIES ]; do
	COUNT=$(($COUNT+1))
	if [ $COUNT -ne 1 ]; then
		echo
		echo "Waiting for Internet connection..."
		false
		until [ $? -eq 0 ]; do
			wget -q --tries=10 --timeout=5 $TURL -O /tmp/probe.txt &> /dev/null
		done
	fi
	echo
	echo "Attempt $COUNT / $MAX_RETRIES"
	echo
	rsync $OPT -e 'ssh -o "ServerAliveInterval 10"' $@
done

FINISH=$SECONDS
if [[ $(($FINISH - $START)) -gt 3600 ]]; then
	ELAPSED="$((($FINISH - $START)/3600))hrs, $(((($FINISH - $START)/60)%60))min, $((($FINISH - $START)%60))sec"
elif [[ $(($FINISH - $START)) -gt 60 ]]; then
	ELAPSED="$(((($FINISH - $START)/60)%60))min, $((($FINISH - $START)%60))sec"
else
	ELAPSED="$(($FINISH - $START))sec"
fi

if [ $COUNT -eq $MAX_RETRIES -a $? -ne 0 ]; then
	echo "Hit maximum number of retries($MAX_RETRIES), giving up. Elapsed time: $ELAPSED"
fi

if [ $? -eq 0 ]; then
	echo "Finished after $COUNT attempt. Elapsed time: $ELAPSED"
fi
