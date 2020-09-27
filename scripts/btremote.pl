#!/usr/bin/perl

# Interact with the bluetooth serial connection providing
# certain status and control information. This is meant for
# on site configuration and debugging when WiFi connection
# is unavailable.  It is mean to be invoked by rfcomm with
# the rfcomm device as its sole argument.
#
# Supported commands:
#    status -- print network status overview
#    ... more to follow

use strict;
use warnings;
use English;
use Carp;

my $device = "/dev/rfcomm0";
my $prompt = "rsked> ";
my $Iface = "wlan0";
my $CheckerUid = 1000;
my $NetstatFile="/run/user/${CheckerUid}/netstat";

#-----------------------------------------------------

# return ip address of the interface $Iface and UP status
#
sub get_status {
    my $wifi_status = qx{/sbin/ip -brief address show $Iface};
    $wifi_status =~ s/\s{2,}/ /msg;
    if (open(my $nsf, '<',  $NetstatFile)) {
	if (defined(my $line1 = <$nsf>)) {
	    chomp($line1);
	    $wifi_status .= "\n$line1";
	}
    }
    return $wifi_status;
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
    elsif ($line=~m/^quit/) {
	print "quit btremote\n";
	last;
    } else {
	print "received unknown command: $line\n";
	print $serial "Unknown command: $line\n";
    }
    print $serial "rsked> ";
}

close $serial or warn "Close failed for $device";

