#!/usr/bin/perl

# This script will produce a JSON summary of audio resources in
# the designated music directory (default: $HOME/Music). This
# resource is used by the rcal web configurator. The assumed
# structure of this directory is:  Artist/Album/Track


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
#########################################################################

use strict;
use warnings;
use Carp;
use Getopt::Long;

# If you don't have JSON::XP, then revert to JSON::PP (in base)
#use JSON::XP;
use JSON::PP;

my $Musicdir = "$ENV{'HOME'}/Music";



# maximum limit on artists to process
# adjust with (--alimit=NNNN)
my $Alimit=20000;

# default (0) emits compressed JSON--much smaller.
# turn on with  --pretty
my $PrettyPrint=0;

my %resources;

## Pick  mediainfo...prefer local one if available.
my $SysMediainfo = "/usr/bin/mediainfo";
my $LocalMediainfo = "/usr/local/bin/mediainfo";
my $Mediainfo;

if (-x $LocalMediainfo) {
    $Mediainfo = $LocalMediainfo;
} elsif (-x $SysMediainfo) {
    $Mediainfo = $SysMediainfo;
} else {
    croak "Unable to find an executable 'mediainfo'";
}
print STDERR "; Using mediainfo: $Mediainfo\n";

#########################################################################

# Collect track durations and total duration (seconds).
# This can be easily collected from an external command:
#   mediainfo --Inform="Audio;%Duration%" <audiofile> => milliseconds
#
sub track_duration_sec {
    my ($mpathname) = @_;  # may have all sorts of shell special chars
    $mpathname =~ s/(["`])/\\$1/g;
    my $msec = qx/$Mediainfo --Inform="Audio;%Duration%" "$mpathname"/;
    chomp $msec;
    if ($msec eq "") {
        print STDERR "Failed to get time for:>>$mpathname<<\n";
        return 0.0;
    }
    return $msec/1000.0
}


# compile files for one album given artist directory and album name
sub skrape_an_album {
    my ($fullpath,$artist,$album) = @_;
    my $ogg_count = 0;
    my $mp3_count = 0;
    my $mp4_count = 0;
    my $enc = "unknown";
    my @songs;
    my @durs;
    my $total_dur = 0.0;
    #
    opendir(my $mdh, $fullpath) or die "Failed to open $fullpath: $!";
    my @filenames = sort readdir($mdh);
    foreach my $song (@filenames) {
        next if $song =~ /^\./;   # ignore hidden files and directories
        my $fpsong = "$fullpath/$song";
        if (-f $fpsong) {
            if ($song =~ /\.ogg$/) { 
                $ogg_count += 1;
                my $dur= track_duration_sec($fpsong);
                $total_dur += $dur;
                push(@songs, $song);
                push(@durs, $dur);
            }
            if ($song =~ /\.mp3$/) {
                $mp3_count += 1;
                my $dur= track_duration_sec($fpsong);
                $total_dur += $dur;
                push(@songs, $song);
                push(@durs, $dur);
            }
            if ($song =~ /\.m4[ab]$/) {
                $mp4_count += 1;
                my $dur= track_duration_sec($fpsong);
                $total_dur += $dur;
                push(@songs, $song);
                push(@durs, $dur);
            }
        }
    }
    my $ncuts = $ogg_count + $mp3_count + $mp4_count;
    if ($ncuts == 0) {
        print "   \"$album/\": $enc $ncuts !!!\n";
        return;
    }
    if ($ogg_count == 0 && $mp3_count==0 && $mp4_count > 0) { $enc = "mp4";}
    if ($ogg_count == 0 && $mp3_count > 0 && $mp4_count == 0) { $enc = "mp3";}
    if ($ogg_count > 0 && $mp3_count==0 && $mp4_count == 0) { $enc = "ogg";}
    $resources{$artist}{$album} = {"tracks" => \@songs, "encoding" => $enc,
                                       "durations" => \@durs,
                                       "totalsecs" => $total_dur};
}

# 
sub skrape_albums {
    my ($fullpath, $artist) = @_;
    #print "\"$artist\"\n";
    $resources{$artist} = {}; # empty hash
    opendir(my $adh, $fullpath) or return;
    while( defined( my $album = readdir($adh)) ) {
        next if $album =~ /^\./;   # ignore hidden files and directories
        my $aapath = "$fullpath/$album";
        if (-d $aapath) {
            skrape_an_album($aapath,$artist,$album);
        }
    }
    closedir($adh);
}

sub print_usage {
    print "Usage: rskrape.pl [--pretty] [--alimit=N] [MusicDir]\n";
    exit(0);
}

sub print_version {
    print "rskrape v1.0, perl5\n";
}

###########################################################################
my $acount=0;

GetOptions("alimit=i"=> \$Alimit,  # numeric
           "pretty" => \$PrettyPrint, # flag
           "help" => sub { print_usage(); },
           "usage" => sub { print_usage(); },
           "version" => sub { print_version(); exit(0); }
           )
    or die("Error in command line arguments\n");

if (scalar(@ARGV)) {
    $Musicdir = $ARGV[0];
}

opendir(my $dh, $Musicdir) or die "Can't open $Musicdir: $!";
while( defined(my $artist = readdir($dh)) ) {
    next if $artist =~ /^\./;   # ignore hidden files and directories
    my $fp = "${Musicdir}/$artist";
    if (-d $fp) {
        skrape_albums( $fp, $artist );
        $acount++
    }
    last if $acount >= $Alimit;
}
closedir($dh);

my $Jason = ($PrettyPrint ? JSON::PP->new->pretty([1]) : JSON::PP->new);
my $utf8_json = $Jason->encode( \%resources );
print "$utf8_json\n";
