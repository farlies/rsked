#!/usr/bin/perl

# Interact with the bluetooth serial connection providing
# certain status and control information. This is meant for
# on site configuration and debugging when WiFi connection
# is unavailable.  It is mean to be invoked by rfcomm with
# the rfcomm device as its sole argument.
#
#
# Note: This needs to be run as root to be able to do WiFi
#    configuration.

use strict;
use warnings;
use English;
use Carp;

my $device = "/dev/rfcomm0";
my $prompt = "rsked> ";
my $Iface = "wlan0";
my $CheckerUid = 1000;
my $NetstatFile="/run/user/${CheckerUid}/netstat";
my $Iwlist = "/sbin/iwlist";

#-----------------------------------------------------

my $Help = <<"EOH";
Supported commands:
  help   : this command
  status : network status
  scan   : scans WiFi networks
  quit   : end session
EOH

#-----------------------------------------------------

# return a multiline string with network information:
#  iwconfig of WiFi interface
#  ip address of the interface $Iface and UP status
#  rsked's  netstat file
#
sub get_status {
    my $wifi_status = qx{/sbin/iwconfig $Iface};
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
	print "status=$sinfo\n";
    }
    elsif ($line=~m/^scan/) {
	print $serial "Starting scan...\n";
	my $wnetworks = scan_wifi();
	print $wnetworks;
	print $serial $wnetworks;
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
    print $serial "rsked> ";
}

close $serial or warn "Close failed for $device";

