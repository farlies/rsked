#!/usr/bin/env perl

# This script will produce a JSON summary of audio resources in the
# designated music directory (default: $HOME/Music). This resource is
# used by the rcal web configurator. The assumed structure of this
# directory is: Artist/Album/Track. If your music library is elsewhere,
# specify its path in the environment variable MUSICDIR, e.g.
#       $ MUSICDIR=/opt/music rskrape.pl > catalog.json
#
# Rskrape will also catalog the playlists (in PlaylistDir/)
# The playlist are assumed to reside in the PlaylistDir,
# by default ~/.config/mpd/playlists; override with environment
# variable PLAYLISTDIR.
#
# Finally, rskrape will include a catalog of announcements if available.
# It looks in the directories ~/.config/rsked/{resources,motd} and
# produces rsked source specifications based on contents and some
# heuristic rules.
#
# The JSON is printed on stdout is structured as an object with 2 fields:
#  {"library" : { artist : { album : { see_below }, ... }, ... },
#   "playlists" : { name.m3u : { "duration": n, "encoding": e}, ... },
#   "announcements" : { %name : { "encoding":e, "text":t, ...} }  }
#
# Each album resource is an object with fields:
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
use POSIX qw(strftime);
use JSON::PP;

# Where to find the Music library.
# Override with environment variable MUSICDIR.
my $Musicdir = $ENV{'MUSICDIR'} // "$ENV{'HOME'}/Music";

# Where to find playlists. Default location is MPD typical.
# Override with environment variable PLAYLISTDIR
my $PlaylistDir = $ENV{'PLAYLISTDIR'} // "$ENV{'HOME'}/.config/mpd/playlists";

print STDERR "; MUSICDIR=$Musicdir\n; PLAYLISTDIR=$PlaylistDir\n";

my $ConfDir="$ENV{'HOME'}/.config/rsked";

# maximum limit on artists to process
# adjust with (--alimit=NNNN)
my $Alimit=20000;

# default (0) emits compressed JSON--much smaller.
# turn on with  --pretty
my $PrettyPrint=0;

# It true, do not harvest playlists
# Disable with flag --noplaylist
my $NoPlaylist=0;

# It true, do not skrape Music library
# Disable with flag --nolibrary
my $NoLibrary=0;

# It true, do not skrape Announcements
# Disable with flag --noannounce
my $NoAnnounce=0;

my %announcements;
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

## Vorbiscomment utility, part of vorbis utils package
my $VorbisComment = "/usr/bin/vorbiscomment";


# print usage information
sub print_usage {
    print "\nUsage: rskrape.pl [options*] [MusicDir]\n";
    print "  [--pretty] [--alimit=N]\n";
    print "  [--noplaylist] [--nolibrary] [--noannounce]\n\n";
    exit(0);
}

# print version of this program
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

# Return the vorbis comment [keyword=comment_text] from ogg file
# given by the full path.  If not found, return arg3: defaultstr.
# The utility 'vorbiscomment' may be used to set or retrieve user
# comments in a vorbis file.  Rsked uses the "Text" comment to
# provide a text version of the audio comment. The alphabetic
# case of the keyword is ignored in matching.
#
sub get_vorbis_comment {
    my ($path,$keyword,$defaultstr) = @_;
    $path =~ s/(["`])/\\$1/g;  # cleanup pathname for shell
    my $comments = qx/$VorbisComment -l "$path"/;
    if ( $comments=~m{ ^ $keyword = (.+) $ }xmi ) {
        return $1;
    }
    print STDERR "; failed to get text for: $path\n";
    return $defaultstr;
}

#########################################################################

# compile files for one album given artist directory and album name
sub skrape_an_album {
    my ($fullpath,$artist,$album) = @_;
    my $ogg_count = 0;
    my $mp3_count = 0;
    my $mp4_count = 0;
    my $flac_count = 0;
    my $enc = "mixed";
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
            if ($song =~ /\.flac$/) {
                $flac_count += 1;
                my $dur= track_duration_sec($fpsong);
                $total_dur += $dur;
                push(@songs, $song);
                push(@durs, $dur);
            }
        }
    }
    my $ncuts = $ogg_count + $mp3_count + $mp4_count + $flac_count;
    if ($ncuts == 0) {
        print STDERR "   \"$album/\": $enc $ncuts !!!\n";
        return;
    }
    if ($ogg_count == 0 && $mp3_count==0
        && $mp4_count > 0 && $flac_count==0) { $enc = "mp4";}
    if ($ogg_count == 0 && $mp3_count > 0
        && $mp4_count == 0 && $flac_count==0) { $enc = "mp3";}
    if ($ogg_count > 0 && $mp3_count==0
        && $mp4_count == 0 && $flac_count==0) { $enc = "ogg";}
    if ($ogg_count==0 && $mp3_count==0
        && $mp4_count == 0 && $flac_count > 0) { $enc = "flac";}

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

###########################################################################


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
    opendir(my $dh, $PlaylistDir) or croak "Can't open $PlaylistDir: $!";
    while( defined(my $plname = readdir($dh)) ) {
        next unless $plname =~ /\.m3u$/;
        process_playlist( $plname );
    }
    closedir($dh);
}


###########################################################################

# add an announcment named rname from subdirectory rdir.
#
sub add_announcement {
    my ($rdir, $rname) = @_;
    my ($fn,$ext) = $rname=~m{ (.*) \. ([^.]+) $}xm;
    my $aname = "%" . "$fn";
    my $enc = $ext;  # normally ogg
    my $fullpath = "$ConfDir/$rdir/$rname";
    my $dur = track_duration_sec( $fullpath );
    my $text = get_vorbis_comment($fullpath,'Text',"?");
    $announcements{$aname} = {
        "encoding" => $enc, "duration" => $dur,
            "location" => "$rdir/$rname", "text" => $text };
}

# announcement scan
#
sub skrape_announcements {
    my $resdir = "$ConfDir/resource";
    my $motdir = "$ConfDir/motd";

    # resources
    opendir(my $dh, $resdir) or croak "Can't open $resdir: $!";
    while( defined(my $rname = readdir($dh)) ) {
        next unless $rname =~ /\.ogg$/;
        add_announcement( "resource", $rname );
    }
    closedir($dh);

    # dynamic message of day
    $announcements{'motd-ymd'} = {
        "encoding" => "ogg", "duration" => 10,
            "location" => "motd/%Y-%m-%d.ogg",
            "dynamic" => $JSON::PP::true,
            "text" => "(content based on calendar day/month/year)" };

    $announcements{'motd-md'} = {
        "encoding" => "ogg", "duration" => 10,
            "location" => "motd/each-%m-%d.ogg",
            "dynamic" => $JSON::PP::true,
            "text" => "(content based on calendar day/month)" };

}

###########################################################################

GetOptions("alimit=i"=> \$Alimit,  # numeric
           "pretty" => \$PrettyPrint, # flag
           "noplaylist" => \$NoPlaylist,
           "nolibrary" => \$NoLibrary,
           "noannounce" => \$NoAnnounce,
           "help" => sub { print_usage(); },
           "usage" => sub { print_usage(); },
           "version" => sub { print_version(); exit(0); }
           )
    or die("Error in command line arguments\n");

if (scalar(@ARGV)) {
    $Musicdir = $ARGV[0];
}

skrape_library() unless $NoLibrary;
skrape_playlists() unless $NoPlaylist;
skrape_announcements() unless $NoAnnounce;

my %result;
if (scalar keys %resources) {
    $result{"library"} = \%resources;
}
if (scalar keys %playlists) {
    $result{"playlists"} = \%playlists;
}
if (scalar keys %announcements) {
    $result{"announcements"} = \%announcements;
}

my $result_version = strftime("%Y-%m-%dZ%H:%M",gmtime);
$result{"encoding"} = "UTF-8";
$result{ "schema"} = "1.0";
$result{"version"} = $result_version;

my $Jason = ($PrettyPrint ? JSON::PP->new->pretty([1]) : JSON::PP->new);
my $utf8_json = $Jason->encode( \%result );
print "$utf8_json\n";

print STDERR "; wrote resources, version $result_version\n";
