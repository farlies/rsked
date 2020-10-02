#!/usr/bin/perl

# Interact with the bluetooth serial connection providing
# certain status and control information. This is meant for
# on site configuration and debugging when WiFi connection
# is unavailable.  It is mean to be invoked by rfcomm with
# the rfcomm device as its sole argument.
#
# $ sudo /usr/bin/rfcomm watch hci0 1 /usr/local/bin/btremote.pl {}
#
# Note: This needs to be run as root to be able to do WiFi
#    configuration.

# To use this:
# 1. Install an app like Serial Bluetooth Terminal (1.33 or later)
#    by Kai Morich.
# 2. Pair your Pi to your phone or tablet.
# 3. Run the rfcomm command above (or use the btremote.service)
# 4. Connect the app to the pi--you should see the prompt.

# Relies on the following external programs, which you must install:
# if not already present:
#
#  rfcomm
#  iwconfig
#  iwlist
#  ip


use strict;
use warnings;
use English;
use Carp;

#-----------------------------------------------------
#                  Configurable Options

# This is the default serial device, if not provided.
my $device = "/dev/rfcomm0";

# User interface on remote terminal emulator
my $prompt = "rsked> ";

# This is the WiFi interface device that btremote will
# assess and potentially configure:
my $Iface = "wlan0";

# external programs
my $Iwlist = "/sbin/iwlist";
my $Iwconfig = "/sbin/iwconfig";
my $Ip = "/sbin/ip";

# The inet checker script produces a netstat script here:
my $CheckerUid = 1000;
my $NetstatFile="/run/user/${CheckerUid}/netstat";

# Log directory and Old Log directory
my $Homedir = "/home/pi";
my $Logdir = "$Homedir/logs";
my $OldLogdir = "$Homedir/logs";

# Default number of lines to tail in log file
my $TailLines = 10;

#-----------------------------------------------------

my $Help = <<"EOH";
Supported commands:
  help   : this command
  last <log> [<n>] : tail logfile
  quit   : end session
  status : network status
  scan   : scans WiFi networks
  warn <log> [<n>] : tail warnings
EOH

#-----------------------------------------------------

# return a multiline string with network information:
#  iwconfig of WiFi interface
#  ip address of the interface $Iface and UP status
#  rsked's  netstat file
#
sub get_status {
    my $wifi_status = qx{$Iwconfig $Iface};
    $wifi_status .= qx{/sbin/ip -brief address show $Iface};
    $wifi_status .= "\n";
    $wifi_status =~ s/\s{2,}/ /msg;
    #
    if (open(my $nsf, '<',  $NetstatFile)) {
	if (defined(my $line1 = <$nsf>)) {
	    chomp($line1);
	    $wifi_status .= "\n$line1";
	}
    }
    return $wifi_status;
}

#-----------------------------------------------------

# Return the last filename in dirname matching filt.  If
# the directory cannot be opened or has no matching files
# then return undef.
#
sub latest_file {
    my ($dirname,$filt) = @_;
    opendir(my $dirh, $dirname) or return undef;
    my @files = grep(/$filt/,readdir($dirh));
    closedir($dirh);
    if (! @files) {
        return undef;
    }
    my @sfiles = sort @files;
    my $fname = pop(@sfiles);
    return "$dirname/$fname";
}

#-----------------------------------------------------

# Get the last n lines of file. If the file cannot be opened then just
# return the empty string.  If it does not have nlines then grab all
# available. If filter is provided, restrict lines to ones matching
# the regular expression.  Returns a string with the selected content.
#
sub tail_file {
    my ($filename,$nlimit,$filt)=@_;
    my $nlines = $nlimit;
    open my $phile,'<',$filename or return("");
    my @lines = <$phile>;
    close($phile);
    if (defined($filt)) {
        @lines = grep(/${filt}/, @lines);
    }
    if ($nlimit > @lines) {
        $nlines = @lines;
    }
    my @tail = splice(@lines,-$nlines);
    return join('',@tail);
}

#-----------------------------------------------------

# command string like:  last <stem> <limit>
#    last
#    last rsked
#    last cool 15
# If the log file root is omitted, assume it is rsked
# If the limit count is omitted, assume it is TailLines.
# If arguments are invalid, return a diagnostic string.
#
sub do_last {
    my ($cmdstr) = @_;
    $cmdstr =~ s/^\s+//;
    my ($cmd, $nom, $lim) = split(/\s+/,$cmdstr);
    if (!defined($nom)) { $nom = "rsked"; }
    if ($nom=~m/[^A-Za-z0-9_-]/) { return "(Illegal log stem)";}
    if (!defined($lim)) {
	$lim = $TailLines;
    } else {
	if (! $lim=~m/^\d+$/xms) {
	    return "bad limit: '$lim'\n";
	}
	$lim = int($lim);
	$lim = 1 if ($lim < 1);
    }
    my $filename = latest_file($Logdir,$nom,$lim);
    if (!defined($filename)) {
	return "(none)\n";
    }
    return tail_file($filename,$lim);
};

#-----------------------------------------------------

my @essids=();

sub parse_cell {
    my ($c,$n) = @_;
    my $essid="?";
    my $crypted="?";
    my $quality="?";
    if ($c=~m/ESSID:("[^"]+")/) {
        $essid = $1;
	push @essids,$essid;
    }
    if ($c=~m/Encryption key:on/) {
        $crypted = "ncryptd";
    }
    if ($c=~m{Quality=(\d+/\d+)}) {
        $quality = $1;
    }
    return " $n. $essid, $crypted, $quality\n";
}


# Return a string with a list of wifi networks 1..n
#
sub scan_wifi {
    my $scan = qx{$Iwlist $Iface scan};
    if (!defined($scan)) {
	return "scan command failed\n";
    }
    my @cells = split(/Cell /,$scan);
    my $cnum = 0;
    my $menu = "";

    @essids = ();
    foreach my $c (@cells) {
	if (0 == $cnum) {
	    if ($c=~m/completed/) {
		$menu .= "Successful Wi-Fi scan\n";
		$cnum += 1;
	    } else {
		$menu .= "Unexpected result: $c\n";
		last;
	    }
	} else {
	    $menu .= parse_cell($c,$cnum);
	    $cnum += 1;
	}
    }
    my $ness = scalar(@essids);
    print "$ness ESSIDs stored\n";
    return $menu;
}

#-----------------------------------------------------

if ($#ARGV >= 0) {
    $device = $ARGV[0];
}

open my $serial, '+<', $device 
    or croak "Cannot open $device : $OS_ERROR";

# autoflush the output
select((select($serial),$|=1)[0]);

print $serial "rsked bt-monitor v0.01\n";
print $serial $prompt;

while (my $line = <$serial>) {
    chomp($line);
    if ($line=~m/^status/) {
	my $sinfo = get_status();
	print $serial "$sinfo\n";
	#print "status=$sinfo\n";
    }
    elsif ($line=~m/^scan/) {
	print $serial "Starting scan...\n";
	my $wnetworks = scan_wifi();
	#print $wnetworks;
	print $serial $wnetworks;
    }
    elsif ($line=~m/^last/) {
	print $serial do_last($line);
    }
    elsif ($line=~m/^help/) {
	print $serial $Help;
    }
    elsif ($line=~m/^quit/) {
	print "quit btremote\n";
	last;
    } else {
	print "received unknown command: $line\n";
	print $serial "Unknown cmd: $line\nFor usage: help\n";
    }
    print $serial $prompt;
}

close $serial or warn "Close failed for $device";

