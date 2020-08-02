#!/bin/bash
#
# Perform a functional test of internet availability.
# Output appears in two (configurable) files:
#  1.  XDG_RUNTIME_DIR/netstat : status line. The first word is an error
#      code; 0 means all is well
#  2.  logs/check_inet.log  : growing log file of exceptions found
#
# Rsked looks at the status line (1) to assess whether network streaming
# should be attempted.
#
# You can run this on cron, e.g. every 5 minutes from 06:00 to 22:00
#
# ----------------------------------------------------------------------
# Part of the rsked package.
# Copyright 2020 Steven A. Harp
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

########################################################################
## Default Configuration:

# which network interface, e.g. wlan0
IFACE=wlan0

## HOST name to check DNS resolver
RHOST=www.streamguys.com

## file to track the index.
## Note: if run in cron the XDG environment variables might not be set.
UCONF="${XDG_RUNTIME_DIR:=/run/user/`id -u`}/lasturl"

## URLs to check...next in list called on each invocation

URLS=("https://www.streamguys.com/robots.txt"
      "https://www.jstor.org/robots.txt"
      "https://www.startribune.com/robots.txt"
      "https://www.theguardian.com/us/robots.txt"
      "https://www.schneier.com/robots.txt"
      "https://www.postgresql.org/robots.txt"
      "https://www.sciencenews.org/robots.txt"
      "https://www.python.org/robots.txt"
      "https://www.digikey.com/robots.txt"
      "https://arstechnica.com/robots.txt"
      "https://www.latimes.com/robots.txt"
      "https://www.reuters.com/robots.txt"
      "https://www.mprnews.org/robots.txt")

url=${URLS[0]}


# where to write the current status line
STATUS_FILE="$XDG_RUNTIME_DIR/netstat"

# where to log problems
LOGFILE="$HOME/logs/check_inet.log"

# If an error is detected, wait this many seconds then retry (once).
FAIL_SECS=5

# Fetch test parameters
WGET=/usr/bin/wget
GETOUT=/tmp/checkfile.out
TIMEOUT=5

########################################################################

# last status code written by this script, if any
LastStatus=0

# Error message var
Err="network appears usable"

# If there is a ci.conf in your rsked/ configuration directory then
# it may override any of the default settings. This file is optional.

# Config file name
CICONFIG="$HOME/.config/rsked/ci.conf"

# Configuration settings in a separate file  will be sourced if present
if [[ -e $CICONFIG ]]; then
    source $CICONFIG
fi


########################################################################
## Functions

# write_status <scode> <message>
#
function write_status
{
    TS=$(date --rfc-3339=seconds)
    if [[ $1 == 0 ]]; then
        status=GOOD
    else
        status=BAD
    fi
    echo $1 $TS $status $2 > $STATUS_FILE
}


# append a message to the log file with a timestamp
#
function logmsg
{
    TS=$(date --rfc-3339=seconds)
    echo $TS $1 >> $LOGFILE
}

# fetch the next URL, saving next index to file UCONF
#
function next_url
{
    nu=${#URLS[@]}
    ii=$(($RANDOM % $nu))
    if [[ -e $UCONF ]]; then
        if read ii; then
            let ii=($ii % $nu)
        fi < $UCONF
    fi
    url=${URLS[$ii]}
    # increment and save
    echo $((++ii % $nu)) > $UCONF
    return 0
}


# Read the last status code into LastStatus.
# If unavailable, the LastStatus is presumed to be 0 (good)
#
function read_last_status
{
    LastStatus=0
    if [[ -e $STATUS_FILE ]]; then
        { read LastStatus etcetera; }  < $STATUS_FILE
        #echo "; Last status = $LastStatus"
    fi
}


# 1. Is the interface in the UP state? Return 0 on success, 1 on failure.
# TODO: Remediation. Bringing up will trigger dhcpd automatically
# but it might take some seconds to get a lease.
#   ip link set $IFACE up
#
function is_up
{
    if ip link show dev $IFACE | grep -q 'state UP'; then
        return 0
    fi
    logmsg "Interface $IFACE is down"
    return 1
}


# 2,3. Does the interface have a valid IP addr? Link Local addresses are
# not considered valid here.
#
function valid_ip
{
    if res=$(ip -4 address show dev $IFACE |
                 grep  -Po  'inet \d+\.\d+\.\d+\.\d+'); then
        # check for useless LL address 169.254.*.*
        if $(echo $res | grep -q ' 169.254.'); then
            logmsg "Link local address found on $IFACE $res"
            return 3
        fi
        return 0
    else
        logmsg "Interface $IFACE lacks an IPv4 address"
        return 2
    fi
}


# 4. Resolver test.
#
function resolver_test
{
    if res=$(dig +noall +answer $RHOST); then
        if echo $res | grep -q -Pq 'IN A'; then
            return 0;
        fi
    fi
    logmsg "Failed to resolve $RHOST"
    return 4;
}


# 5. Try fetching a small file from the next source (to destination
# $GETOUT). If necessary, try all URLs before failing (since some
# problems may be transitory).  Return value is exit status of the
# wget; 0 if no problems.
#
function fetch_test
{
    nu=${#URLS[@]}
    for (( i=0; i < $nu ; i++ ))
    do
        next_url
        if $WGET -q -t 2 --backups=2 -T $TIMEOUT -O $GETOUT \
                 -a $LOGFILE $url; then
            return 0
        else
            logmsg "fetch_test failed on $url"
        fi
    done
    logmsg "All $nu URLs failed"
    return 5
}


# Run tests in order until one fails or all succeed.
#
function all_tests
{
    if ! is_up; then
        Err="ifup test failed"
        return 1
    fi
    if ! valid_ip; then
        Err="valid IP test failed"
        return 2
    fi
    if ! resolver_test; then
        Err="resolver_test failed"
        return 4
    fi
    if ! fetch_test; then
        Err="fetch_test failed"
        return 5
    fi
    Err="network appears usable"
    return 0
}


########################################################################
# Run tests; returns 0 so long as it can at least run the tests.

read_last_status

if all_tests; then
    write_status 0 $Err
    if [[ $LastStatus != 0 ]]; then
        logmsg "Network usable again"
    fi
    exit 0
fi

# Failure. Some problems are transitory. Wait briefly, then retry.
# echo "; $Err-- waiting $FAIL_SECS seconds before trying again"
sleep $FAIL_SECS

# Final test result
all_tests
write_status $? $Err
if [[ $LastStatus != 0 ]]; then
    logmsg "Network usable again"
fi
