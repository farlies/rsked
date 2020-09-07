#!/usr/bin/perl

# This script will produce a JSON summary of audio resources in
# the designated music directory (default: $HOME/Music). This
# resource is used by the rcal web configurator. The assumed
# structure of this directory is:  Artist/Album/Track
# It will also catalog the playlists (in PlaylistDir/)
#
# The JSON is printed on stdout is structured as an object with 2 fields:
#  {"library" : { artist : { album : { see_below }, ... }, ... },
#   "playlists" : { name.m3u : { "duration": n, "encoding": e}, ... } }
#
# Each album is an object with fields:
#   "tracks" : array of filenames
#   "durations" :array of track durations in seconds
#   "encoding" : one of ogg, mp3, mp4
#   "totalsecs" : sum of all track durations

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

# Where to find playlists. Default location is MPD typical.
my $PlaylistDir = "$ENV{'HOME'}/.config/mpd/playlists";


# maximum limit on artists to process
# adjust with (--alimit=NNNN)
my $Alimit=20000;

# default (0) emits compressed JSON--much smaller.
# turn on with  --pretty
my $PrettyPrint=0;

# Harvest playlists instead of Music library
# enable with flag --playlists
my $SkrapePlaylists=1;

my %resources;
my %playlists;

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


sub print_usage {
    print "Usage: rskrape.pl [--pretty] [--alimit=N] [MusicDir]\n";
    exit(0);
}

sub print_version {
    print "rskrape v1.0, perl5\n";
}


#########################################################################

# Collect track durations and total duration (seconds).
# This can be easily collected from an external command:
#   mediainfo --Inform="Audio;%Duration%" <audiofile> => milliseconds
# We will not attempt to do this for URLs, instead return 0.
#
sub track_duration_sec {
    my ($mpathname) = @_;  # may have all sorts of shell special chars
    if ($mpathname=~m{http(s)*://}) {
        print STDERR "Won't measure: $mpathname\n";
        return 0;
    }
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
            if ($song =~ /\.(m4a|m4b|mp4)$/) {
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

# library scan
#
sub skrape_library {
    my $acount=0;
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
}

# If argument is a relative path, prepend the Musicdir.
# Return possibly prefixed arg.
#
sub abspath {
    my ($fn) = @_;
    if ($fn=~m{^/}) { return $fn; }  # absolute path
    return "$Musicdir/$fn";
}

# process one playlist
#
sub process_playlist {
    my ($plname) = @_;
    my $plpath = "$PlaylistDir/$plname";
    open my $plfile, '<', $plpath
        or croak "Cannot open $plpath";
    my @lines = <$plfile>;
    close $plfile;
    my ($nogg,$nmp3,$nmp4,$dur)=(0,0,0,0);
    foreach my $res (@lines) {
        next if $res=~m/^\s*#/;
        chomp $res;
        if ($res=~m/\.ogg/) { 
            $nogg +=1; $dur += track_duration_sec(abspath($res)); }
        elsif ($res=~m/\.mp3/) {
            $nmp3 +=1;  $dur += track_duration_sec(abspath($res)); }
        elsif ($res=~m/\.(mp4|m4a|m4b)/) {
            $nmp4 +=1;  $dur += track_duration_sec(abspath($res)); }
    }
    my $ntotal = $nogg+$nmp3+$nmp4;
    return unless $ntotal;
    my $enc = "mixed";
    if ($nogg == $ntotal) {
        $enc = "ogg";
    } elsif ($nmp3 == $ntotal) {
        $enc = "mp3";
    } elsif ($nmp4 == $ntotal) {
        $enc = "mp4";
    }
    #print STDERR "$plname: $enc, Total sec: $dur\n";
    $playlists{$plname} = { "encoding" => $enc, "duration" => $dur };
}


# playlist scan
#
sub skrape_playlists {
    opendir(my $dh, $PlaylistDir) or die "Can't open $PlaylistDir: $!";
    while( defined(my $plname = readdir($dh)) ) {
        next unless $plname =~ /\.m3u$/;
        process_playlist( $plname );
    }
    closedir($dh);
}

###########################################################################

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

skrape_library();
skrape_playlists();

my %result;
if (scalar keys %resources) {
    $result{"library"} = \%resources;
}
if (scalar keys %playlists) {
    $result{"playlists"} = \%playlists;
}

my $Jason = ($PrettyPrint ? JSON::PP->new->pretty([1]) : JSON::PP->new);
my $utf8_json = $Jason->encode( \%result );
print "$utf8_json\n";
