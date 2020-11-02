#!/usr/bin/perl

# btremote.pl
#
#   Part of the rsked package.
#   Copyright 2020 Steven A. Harp   farlies(at)gmail.com
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

# Interact with the bluetooth serial connection providing
# certain status and control information. This is meant for
# on site configuration and debugging when WiFi connection
# is unavailable.  It is mean to be invoked by rfcomm with
# the rfcomm device as its sole argument.
#
# $ sudo /usr/bin/rfcomm watch hci0 1 /usr/local/bin/btremote.pl {}
#
# Note: This needs to be run as root to be able to execute some
#    commands. This is of course dangerous, so don't use
#    this in environments where unauthorized parties might have
#    Bluetooth access to the device. Authentication: TBD.

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
#  wpa_cli
#  wpa_passphrase
#  ----------------------

use strict;
use warnings;
use English;
use Carp;
use Text::ParseWords;

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
my $Shutdown = "/sbin/shutdown";

my $Sysctl = "/bin/systemctl"; # Debian Buster
if (! -e $Sysctl) {
    $Sysctl = "/bin/systemctl"; # Ubuntu
}

my $Wpapass = "/usr/bin/wpa_passphrase";
my $Wpacli = "/sbin/wpa_cli";   # Debian Buster
if (! -e $Wpacli) {             # Ubuntu
    $Wpacli = "/usr/sbin/wpa_cli";
}


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

# List of SSIDs of visible WiFi networks (after scan)
my @essids=();


# Keep these short since few columns are available on
# a typical phone screen serial terminal....

my $Help = <<"EOH";
Supported commands:
  help   : this command
  boot   : reboot now
  halt   : power off device now
  last <lg> [<n>] : tail logfile
  quit   : end session
  status : network status
  time   : print date/time
  warn <lg> [<n>] : tail warnings
  wadd <ssid> <psk>  : add WiFi net
  watt    : attach WiFi
  wls     : list stored WiFi nets
  wrm <j> : remove stored WiFi net
  wscan   : scan visible WiFi nets
EOH

#-----------------------------------------------------

# print diagnostics to console if any elements of the kit seem misconfigured
#
sub check_kit {
    my @execs = ($Iwconfig, $Iwlist, $Ip, $Sysctl, $Shutdown, $Wpapass, $Wpacli);
    for my $x (@execs) {
	if (! -x $Iwlist) {
	    print "WARNING: $x is unavailable\n";
	}
    }
    # TODO: other checks...
}

#-----------------------------------------------------



# return code output. If not 0, print message
#
sub diag_child {
    my $rc = $?;
    if ($rc == -1) {
	print "failed to execute: $!\n";
    }
    elsif ($rc & 127) {
	printf "child died with signal %d, %s coredump\n",
	    ($? & 127),  ($? & 128) ? 'with' : 'without';
    }
    else {
	$rc = $? >> 8;
	if ($rc) {
	    print "child exited with value $rc\n";
	}
    }
    return $rc;
}

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

# extract the ssid, quality and crypto status for a cell
# This will also add the ssid to the array @essids.
#
sub parse_cell {
    my ($c,$n) = @_;
    my $essid="?";
    my $crypted="?";
    my $quality="?";
    if ($c=~m/ESSID:"([^"]+)"/) {
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


# Return a string with a list of visible WiFi networks 1..n
# The global array @essids contains their names.
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


# return the new netid
sub make_new_network {
    my ($ssid) = @_;
    # print "'$ssid' is not yet known, adding...\n";
    # add a network
    my $result = qx{$Wpacli add_network};
    my ($n) = $result=~m{^ (\d+) }xms;
    # print "\nResult = $result \n";
    if (defined($n)) {
	print "$ssid is net '$n'\n";
    } else {
	print "Failed to get a net id!";
	diag_child();
    }
    return $n;
}

# Encode the passphrase and return the ciphertext
#
sub encode_psk {
    my ($ssid, $psk) = @_;
    if ( length($psk) < 8 ) {
	print "Nope: passphrase too short\n";
	return 0;
    }
    my $dpsk = $psk=~s{'}{'\\''}gr;
    my $dssid = $ssid=~s{'}{'\\''}gr;
    #print qq{$Wpapass '$dssid' '$dpsk'};
    #
    my $resp = qx{$Wpapass '$dssid' '$dpsk'};
    if (diag_child()) {
	print "failed to encode psk\n";
	print $resp if (defined($resp));
	return(0);
    }
    if ($resp=~m{ ^ \t psk=([[:xdigit:]]+) \n }xms) {
	return $1;
    }
    return 0;
}

# Set the preshared key to ps for network number $psk;
# return 0 on success, -1 on failure.
#
sub set_wifi_passwd
{
    my ($netid,$ssid,$psk) = @_;
    my $dpsk = encode_psk( $ssid, $psk );
    return (-1) if (! $dpsk);
    # print qq{$Wpacli set_network $netid psk $dpsk\n};
    my $res = qx{$Wpacli set_network $netid psk $dpsk};
    if (diag_child()) {
	print "failed to set psk\n";
	print $res if (defined($res));
	return(-1);
    }
    return 0;
}

# Enable the network with given ID. Return 0 on success.
#
sub enable_network
{
    my ($netid) = @_;
    my $res = qx{$Wpacli enable_network $netid};
    if (diag_child()) {
	print "failed to enable network $netid\n";
	print $res if (defined($res));
	return(-1);
    }
    return 0;
}

# Save the configuration file.
sub save_wpa_config {
    my $res = qx{$Wpacli save_config};
    if (diag_child()) {
	print "failed to save the configuration\n";
	print $res if (defined($res));
	return(-1);
    }
    return 0;
}

# Remove  the network with given ID. Return 0 on success.
#
sub remove_network
{
    my ($netid) = @_;
    my $res = qx{$Wpacli remove_network $netid};
    if (diag_child()) {
	return "failed to remove network $netid\n";
    }
    return 0;
}


# attempt to remove network <netid> {0,1,2,...}
# return a string listing remaining networks
# e.g.
#     wrm 0
#
sub remove_net_cmd {
    my ($line) = @_;
    if ($line=~m{^ wrm \s+ ( \d )+ $}xms) {
	my $netid = $1;
	if (my $res = remove_network($netid)) {
	    return $res;
	}
	save_wpa_config();
	return qx{$Wpacli list_networks};
    } else {
	return "Usage: wrm <netid>\n";
    }
}

# Usage: add_wifi( ssid, psk )
#
sub add_wifi {
    my ($ssid, $psk) = @_;
    my $netid;
    my @mynets = qx{$Wpacli list_networks};
    foreach my $line (@mynets) {
        my ($nid,$kssid) = $line=~m{^ (\d+) \t+ ([^\t]+) \t+ }xms;
        if (defined($nid)) {
            if ($ssid eq $kssid) {
                $netid = $nid;
            }
        }
    }
    if (!defined($netid)) {
	$netid = make_new_network($ssid);
	return if (! defined($netid));
	my $res = qx{$Wpacli set_network $netid ssid '"$ssid"'};
	if (diag_child()) {
	    return "failed to set ssid to $ssid\n";
	}
    }
    # Set psk, enable, and save
    if ( set_wifi_passwd( $netid, $ssid, $psk )
	 || enable_network( $netid )
	 || save_wpa_config() ) {
	remove_network($netid);
	return "failed to register";
    }
    my $netlist = qx{$Wpacli list_networks};
    return $netlist;
}

# watt command.  Kick dhcpc to get an address on some local wifi
# This can take a few seconds...
#
sub wifi_attach {
    # bring wlan0 UP (just in case)
    my $wifi_up = qx{$Ip link set $Iface up};
    if (diag_child()) {
        return "failed to UP $Iface\n";
    }

    # restart dhcpcd.service -- seemingly needed to attach
    my $restart = qx{$Sysctl restart dhcpcd};
    if (diag_child()) {
        return "failed to restart dhcpcd\n";
    }
    return "restarted $Iface";
}


# Register a WiFi network given either an ssid or a scanned index.
# The sole argument is the user command line, e.g.
#    wadd  <n>   <psk>
#    wadd <essid> <psk>
# The arguments <psk> and <essid> must be in quotes if they contain
# any white space or special characters. Some pathological cases won't be
# handled; the standard imposes no restrictions on bytes that may appear
# in an SSID, e.g. might be all unprintable characters.
# Returns a diagnostic string.
#
sub add_wifi_cmd {
    my @qwords = shellwords(@_);
    if (3 != @qwords) {
	return "Wrong number of arguments";
    }
    if ($qwords[0] ne "wadd") {
	return "Wrong command?";
    }
    my $ssid = $qwords[1];
    my $psk = $qwords[2];
    # possibly treat ssid as a 1-based index <n> into scanned essids
    if ($ssid=~m{^ \d+ $}xms) {
	my $idx = ($ssid - 1);
	if (($idx >= 0) && ($idx <= $#essids)) {
	    $ssid = $essids[$idx];
	    print "Selecting scan SSID: $ssid\n";
	}
	# else it might be an SSID in the form of a number (ugh)
    }
    return add_wifi( $ssid, $psk )
}



# Return a string with a list of known wifi networks.
#
sub known_wifi_nets {
    my $knets = qx{$Wpacli list_networks};
    if (diag_child()) {
	return "failed to retrieve networks\n";
    }
    return $knets;
}


#-------------------------------------------------------------------------------

check_kit();

if ($#ARGV >= 0) {
    $device = $ARGV[0];
}

open my $serial, '+<', $device 
    or croak "Cannot open $device : $OS_ERROR";

# autoflush the output
select((select($serial),$|=1)[0]);

print $serial "rsked BTremote v0.1\n";
print $serial scalar(localtime),"\n";
print $serial $prompt;

while (my $line = <$serial>) {
    chomp($line);
    if ($line=~m/^status/) {
	my $sinfo = get_status();
	print $serial "$sinfo\n";
	#print "status=$sinfo\n";
    }
    elsif ($line=~m/^last/) {
	print $serial do_last($line);
    }
    elsif ($line=~m/^help/) {
	print $serial $Help;
    }
    elsif ($line=~m/^wadd/) {
	print $serial add_wifi_cmd( $line );
    }
    elsif ($line=~m/^watt/) {
	print $serial wifi_attach();
    }
    elsif ($line=~m/^wls/) {
	print $serial known_wifi_nets();
    }
    elsif ($line=~m/^wrm/) {
	print $serial remove_net_cmd($line);
    }
    elsif ($line=~m/^wscan/) {
	print $serial "Starting scan...\n";
	my $wnetworks = scan_wifi();
	print $serial $wnetworks;
    }
    elsif ($line=~m/^halt/) {
	print $serial "Halting system NOW\n";
	my $result = qx{$Shutdown -h now};
	print $serial $result;
    }
    elsif ($line=~m/^boot/) {
	print $serial "Rebooting system NOW\n";
	my $result = qx{$Shutdown -r now};
	print $serial $result;
    }
    elsif ($line=~m/^time/) {
	print $serial scalar(localtime),"\n";
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

