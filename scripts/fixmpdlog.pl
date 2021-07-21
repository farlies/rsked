#!/usr/bin/perl
#
# Usage:  perl -i fixmpdlog.pl  logs/mpd.1.log
#
# Description: Standardize an mpd log file for viewing in the browser.
#
# Note that mpd does not (as of 0.21) write a full date with a year,
# so old logs will default to the current year. Nor does the
# timestamp include seconds, so this field is omitted.

# Needs Date::Manip date parsing routines. On Debian, install e.g.
# via:    sudo apt install libdate-manip-perl

use strict;
use warnings;

use Date::Manip qw(ParseDate UnixDate);


# Filters out "update" lines, i.e. scanning of the music library.

sub fix_line {
    my $lvl = "info";
    my $year = 2021;
    my $line = shift; # MMM     DD       HH    mm
    if ( $line =~ m{^ (\w+ \s \d+ \s \d\d:\d\d) \s : \s+ ([^:]+)
         : \s* (.*) $}xms ) {
        my ($dtime,$fac,$msg) = ($1,$2,$3);
        return if ($fac eq 'update');
        my $daet = ParseDate($dtime, FUZZY => 1, PREFER_PAST => 1);
        my $realdtime = UnixDate($daet,"%Y-%m-%d %H:%M");
        if ($fac eq 'exception') {
            $fac = "mpd";
            $lvl = "error";
        }
        print "$realdtime <$lvl> [$fac] $msg\n";
    } else {
        print "??? $line\n";
    }
}

while (<>) {
    chomp;
    fix_line( $_);
}
