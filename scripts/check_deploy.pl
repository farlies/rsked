#!/usr/bin/perl
#
# This script will check a variety of configuration features on
# the local machine to determine if it is ready for deployment.
# It should be run while no other rsked software is running
# but the autostart script is in place (to launch on next boot).
#
# To customize the checks, add a file:
#     ~/.config/rsked/deploy_check.pl
# which sets parameters as listed below.

use strict;
use warnings;
use Term::ANSIColor qw(:constants);
use IO::Socket::INET;

my $ProgVersion = "RSKED check_deploy v0.1";

my $HOME = $ENV{"HOME"};
my $BIN = "$HOME/bin";
my $CFG = "$HOME/.config";


########################################################################
# Configuration parameters that might change per deployment
# ***   Override these in ~/.config/rsked/deploy_check.pl    ***

# Should this deployment have WiFi internet? (Some deployments wish no
# network connections.)
#
my $WANTS_INET = 1;

my $ESSID = "Guest Wifi";

my $WIFI = "wlan0";

# quietest allowed master volume setting (%)
my $MinVol = 50;

# real time clock (battery backup CMOS clock)
my $RTC = "/dev/rtc0";

# Minimum free space allowed on root device
my $MinMB = 100;

# Use this gqrx
my $GQRXBIN = "$BIN/gqrx";


# vendor:product hex code for the USB radio device
my $SDR = "0bda:2838";   # Realtek Semiconductor Corp. RTL2838 DVB-T

# This file should have an entry to run startup_rsked == ASINVOKE
my $AUTOSTART="$CFG/lxsession/LXDE-pi/autostart";
my $ASINVOKE="\@${BIN}/startup_rsked";


# This file can override any of the above options:
do "$CFG/rsked/deploy_check.pl";

########################################################################

my $Deploy_warnings=0;
my $Deploy_errors=0;

my @LOGDIRS = ("$HOME/logs", "$HOME/logs_old");

my @Req_binaries=( "$BIN/check_inet.sh", 
                   "$BIN/cooling",
                   "$BIN/gpiopost.py",
                   "$GQRXBIN",
                   "$BIN/rsked",
                   "$BIN/startup_rsked",
                   "$BIN/vumonitor",
                   "/usr/bin/amixer",
                   "/usr/bin/dig",
                   "/usr/bin/mpc",
                   "/usr/bin/mpd",
                   "/usr/bin/mpg321",
                   "/usr/bin/ogg123",
                   "/usr/bin/pkill" );

# canonical mpd config file
my $MPDCONF="$CFG/mpd/mpd.conf";

# required configuration files
my @Req_configs=( "$CFG/rsked/rsked.json",
                  "$CFG/rsked/schedule.json",
                  "$CFG/rsked/ci.conf",
                  "$CFG/rsked/cooling.json",
                  "$CFG/gqrx/gold.conf",
                  $MPDCONF);

# required directories
my @Req_dirs=( @LOGDIRS,
               "$CFG/rsked/motd",
               "$CFG/rsked/resource",
               "$CFG/mpd/playlists" );

#######################################################################


sub print_passed {
    my ($test, $message) = @_;
    print GREEN BOLD "Passed", RESET, " [$test] $message\n";
}

sub print_warning {
    my ($test, $message) = @_;
    print BOLD YELLOW "Warning", RESET, " [$test] $message\n";
}

sub print_failed {
    my ($test, $message) = @_;
    print BOLD RED "Failed", RESET, " [$test] $message\n";
}

sub print_info {
    my ($test, $message) = @_;
    print BLUE BOLD "Inform", RESET, " [$test] $message\n";
}


# E.g. print_log_matches( 'error', 'rsked', $logstring )
#
sub print_log_matches {
    my ($level,$facility,$str) = @_;
    while ($str=~m{ < $level > \s+ \[ $facility \] \s+ ([^\n]+) }xmsg) {
        print RED "  **** ", RESET, "$1\n";
    }
    while ($str=~m{ \[ $level \] \s+ ([^\n]+) }xmsg) {
        print RED "  **** ", RESET, "$1\n";
    }
}

# Read a whole file as a string (without use of non-standard modules).
#
sub slurp_file {
    my ($file) = @_;
    open my $fh, '<', $file or return("");
    my $oldifs = $/;
    $/ = undef;
    my $data = <$fh>;
    close $fh;
    $/ = $oldifs;
    return($data);
}

# Return the number of real files in the directory argument.
#
sub count_files {
    my ($dname) = @_;
    if (opendir(my $dh, $dname)) {
        my @files = readdir($dh);
        return (scalar(@files) - 2); # dont count . and ..
    } else {
        return 0;
    }
}

# verify systemd service is active and running
sub check_service_active {
    my ($svc) = @_;
    my $status = qx/systemctl status $svc/;
    if ($status =~ m{active \s+ \(running\)}xms) {
        return 1;
    }
    return 0;
}

# Try to open the port on the host: return 1 on success, 0 on failure.
# An open socket is immediately closed.
#
sub can_connect_tcp {
    my ($host,$port) = @_;
    my $socket = IO::Socket::INET->new(PeerAddr => $host,
                                       PeerPort => $port,
                                       Proto => "tcp",
                                       Type => SOCK_STREAM);
    if ($socket) {
        close($socket);
        return 1;
    }
    return 0;
}

#######################################################################

# check that rfkill has not blocked wifi or bluetooth
#
sub verify_no_rfkill {
    my $rfko = qx/rfkill/;
    if ($rfko=~m{wlan \s+ phy0 \s+ unblocked \s+ unblocked}xms) {
        print_passed("rfkill","WiFi radio: enabled")
    } else {
        print_failed("rfkill","WiFi radio: disabled or absent");
        $Deploy_errors += 1;
    }
    if ($rfko=~m{bluetooth \s+ hci0 \s+ unblocked \s+ unblocked}xms) {
        print_passed("rfkill","Bluetooth radio: enabled")
    } else {
        print_failed("rfkill","Bluetooth radio: disabled or absent");
        $Deploy_errors += 1;
    }
}

#######################################################################

# We assume that a virtual framebuffer must be running to provide an
# X screen on a Pi that has no hdmi screen.
#
sub verify_xvfb {
    my $fbpid = qx/pgrep Xvfb/;
    if ($fbpid=~m/\d+/) {
        print_passed("Xvfb","virtual framebuffer is running");
    } else {
        print_failed("Xvfb","No sign of virtual framebuffer");
        $Deploy_errors += 1;
    }
}


#######################################################################

# Verify that the wifi interface exists and has the needed ESSID
#
sub verify_wifi {
    my $iwc = qx{iwconfig $WIFI 2>/dev/null};
    if (! $iwc) {
        print_failed("wifi","No such device: $WIFI");
        return;
    }
    my ($essid) = $iwc=~m/ESSID:"(\S+)"/;
    if (defined($essid) && ($essid eq $ESSID)) {
        print_passed("wifi","Interface $WIFI is connected: '$essid'");
    } else {
        print_warning("wifi","$WIFI exists, but not connected to '$ESSID'");
        $Deploy_warnings += 1;
        verify_wpa();
    }
}

sub verify_wpa {
    my $supconf = "/etc/wpa_supplicant/wpa_supplicant.conf";
    if (! -e $supconf) {
        print_failed("wpa","Missing WPA supplicant file: $supconf");
        return;
    }
    my $content = slurp_file($supconf);
    if ( $content=~m{ssid = "$ESSID"}xms ) {
        print_passed("wpa.key","WPA has a key for '$ESSID'");
    } else {
        print_failed("wpa.key","WPA lacks a key for '$ESSID'");
        $Deploy_errors += 1;
    }
    if (check_service_active("wpa_supplicant.service")){
        print_passed("wpa.sup","wpa supplicant service running");
    } else {
        print_failed("wpa.sup","wpa supplicant service NOT running");
        $Deploy_errors += 1;
    }
}

#######################################################################

# Ensure there is a sound card in the machine.
#
sub verify_sound_card {
    my $cardstr = slurp_file("/proc/asound/cards");
    if ($cardstr) {
        my ($id,$name)
            = $cardstr=~m{^ \s* (\d+) \s+ \[ [^\]]+ \] : \s+ ([^\n]*) \n}xms;
        if (defined($id)) {
            print_passed("audio", "Audio card $id : $name");
        } else {
            print_failed("audio", "Cannot find an audio card");
            $Deploy_errors += 1;
        }
    }
}



# This takes the output of amixer get Master playback and
# returns two values: the volume (%) and the channel 'on' status.
# If the channel cannot be found, the return values are undef.
#
sub get_mixer_vol {
    my ($mixr,$Chan) = @_;
    my ($vol,$db,$on) =
        $mixr=~m{ $Chan : \s+ Playback \s+ \d+ \s+ \[ (\d+)% \]
                         \s+ (\[ \S+ dB \] \s+){0,1} \[ (\w+) \]}xms;
    return ($vol, $on);
}

# The master left/right/mono need to be ON and at least MinVol
#
sub verify_amixer {
    my $mixr = qx{/usr/bin/amixer get Master playback};
    my ($lvol,$lon) = get_mixer_vol($mixr,"Left");
    my ($rvol,$ron) = get_mixer_vol($mixr,"Right");
    my ($mvol,$mon) = get_mixer_vol($mixr,"Mono");
    if (defined($lvol) && defined($rvol)
        && ($lvol >= $MinVol) && ($rvol >= $MinVol)
        && ($lon eq 'on') && ($ron eq 'on')) {
        print_passed("amixer","Left and Right channels are ON @ $lvol/$rvol");
    }
    elsif ( defined($mvol) && ($mon eq 'on') && ($mvol >= $MinVol) ) {
            print_passed("amixer","Mono channel is ON @ $mvol");
    }
    else {
        print_failed("amixer","Mono/Left/Right channels off or too quiet");
        $Deploy_errors += 1;
    }
}


#######################################################################

sub verify_binaries {
    my $nmiss=0;
    foreach my $binfile (@Req_binaries) {
        if (! -x $binfile) {
            print_failed("binaries","$binfile missing or non-executable");
            $nmiss += 1;
            $Deploy_errors += 1;
        }
    }
    if (! $nmiss) {
        print_passed("binaries","all present and executable");
    }
}

#######################################################################

sub verify_configs {
    my $test = "configs";
    my $nmiss=0;
    foreach my $cfgfile (@Req_configs) {
        if (! -r $cfgfile) {
            print_failed($test,"'$cfgfile' missing or non-readable");
            $nmiss += 1;
            $Deploy_errors++;
        }
        if (-z $cfgfile) {
            print_failed($test,"'$cfgfile' is empty!");
            $nmiss += 1;
            $Deploy_errors++;
        }
    }
    if (! $nmiss) {
        print_passed($test,"config files all present and readable");
    }
}

#######################################################################

# Directories must exist and be writable.
#
sub verify_dirs {
    my $test = "dirs";
    my $nmiss=0;
    foreach my $dir (@Req_dirs) {
        if (! -d $dir || ! -w $dir) {
            print_failed($test,"'$dir' missing or non-writable");
            $nmiss += 1;
            $Deploy_errors++;
        }
    }
    if (! $nmiss) {
        print_passed($test,"data directories all present and writable");
    }
}


#######################################################################


sub verify_logs {
    my $test = "logs";
    foreach my $ldir (@LOGDIRS) {
        if (-d $ldir) {
            my $nef = count_files($ldir);
            if ($nef) {
                print_warning($test,"Directory $ldir is not empty ($nef)");
                $Deploy_warnings++;
            }
        }
    }
}


#######################################################################

# The configured gqrx should be our custom fork that respects DSP1 command.
#
sub verify_gqrx {
    my $test="gqrx";
    if (! -x $GQRXBIN) {
        print_failed($test,"Missing SDR player: $GQRXBIN");
        $Deploy_errors++;
        return;
    }
    qx{grep -q DSP1 $GQRXBIN};
    if ($?) {
        print_failed($test,"wrong version of gqrx: $GQRXBIN");
        $Deploy_errors++;
        return;
    }
    print_passed($test,"compatible version of gqrx found");
}


#######################################################################

# Should have a real hardware clock, and the fake hwclock service
# should be disabled.
#
sub verify_hwclock {
    my $test="hwclock";
    if (! -c $RTC) {
        print_warning($test,"Missing hardware clock device $RTC");
        $Deploy_errors++;
        return;
    }
    print_passed($test,"Found hardware clock $RTC");
    #
    if (check_service_active("fake-hwclock.service")) {
        print_failed($test,"Fake hwclock is still enabled");
    }
    print_passed($test,"Fake hwclock absent or disabled");
}


#######################################################################

# Verify that the radio device is plugged into USB.
#
sub verify_sdr {
    my $sdr = qx/lsusb -d $SDR/;
    if (! defined($sdr) || ! $sdr) {
        print_failed("sdr","failed to find SDR device $SDR");
        $Deploy_errors++;
        return;
    }
    my ($a,$dev) = split(/$SDR/,$sdr);
    print_passed("sdr","Found device: $dev");
}

#######################################################################

# Verify at least MinMB space is left (Megabytes) on the root device
# df -BM --output=avail,target
#  95126M /
#
sub verify_space {
    my $dfout = qx{df -BM --output=avail,target};
    if ( defined($dfout) ) {
        if ($dfout=~m{^ \s* (\d+)M \s+ / $}xms) {
            my $mb = $1;
            if ($mb < $MinMB) {
                print_failed("space","Insufficient free space: $mb MB");
                $Deploy_errors++;
                return;
            } else {
                print_passed("space","Adequate free space: $mb MB");
                return;
            }
        }
    }
    print_warning("space","Unable to assess free space on root device...");
}


#######################################################################

# Verify that startup_rsked is invoked inside autostart.
#
sub verify_autostart {
    my $test="autostart";
    if (! -r $AUTOSTART) {
        print_failed($test,"Missing autostart file: $AUTOSTART");
        return;
    }
    my $ascontent = slurp_file($AUTOSTART);
    if ($ascontent=~m{^ $ASINVOKE $}xms) {
        print_passed($test,"startup_rsked will be invoked on session start");
    } else {
        print_failed($test,"startup_rsked isn't invoked in $AUTOSTART");
        $Deploy_errors++;
    }
}

#######################################################################

# crontab should [or should not] have a job to check_inet
#
sub verify_crontab {
    my $test = "crontab";
    my $cron = qx/crontab -l/;
    my $running_chk = ($cron=~m/check_inet.sh/);
    if ($WANTS_INET) {
        if ($running_chk) {
            print_passed($test,"running check_inet");
        } else {
            print_failed($test,"not running check_inet");
            $Deploy_errors++;
            return;
        }
    } else {
        if ($running_chk) {
            print_failed($test,"running check_inet in a NO-inet deployment");
            $Deploy_errors++;
            return;
        } else {
            print_passed($test,"running check_inet");
        }
    }
}


#######################################################################

# Verify that mpd is NOT running and port/socket are free.
#
sub verify_nompd {
    my $test = "nompd";
    if (! -r $MPDCONF) {
        print_failed($test,"Missing file: $MPDCONF");
        $Deploy_errors++;
        return;
    }
    my $conf = slurp_file($MPDCONF);
    my $sock = "/run/user/1000/mpd.socket";
    my $port = 6600;

    while ( $conf=~m{^ bind_to_address \s+ " ([^"]+) " $}xmsg ) {
        my $btoa = $1;
        if ($btoa=~m/sock/) {
            $sock = $btoa;
            print_info($test,"mpd config socket = '$sock'");
        }
    }
    while ( $conf=~m{^ port \s+ " (\d+) " $}xmsg ) {
        $port = $1;
        print_info($test,"mpd config port = $port");
    }

    if (-e $sock) {
        print_failed($test,"mpd socket currently exists--rogue mpd?");
        $Deploy_errors++;
        return;
    }
    if (can_connect_tcp("localhost",$port)) {
        print_failed($test,"mpd $port/tcp socket currently connectable--rogue mpd?");
        $Deploy_errors++;
        return;
    }
    print_passed($test,"No rogue mpd currently running.");
}

#######################################################################

# Run rsked in test mode. If it returns 0 we are okay.
#
sub verify_rsked {
    my $test = "rsked";
    my $binar = "$HOME/bin/rsked";
    my $log = qx{$binar --test 2>&1 1>/dev/null};
    # Diagnose
    if ($? == -1) {
        print_failed($test, "failed to execute: $!");
        $Deploy_errors += 1;
    }
    elsif ($? & 127) {
        my $sigexit = ($? & 127);
        if ($sigexit != 0) {
            print_error($test, "$! died with signal $sigexit");
            $Deploy_errors += 1;
            return;
        }
    } else {
        my $exitcode = ($? >> 8);
        if ($exitcode == 0) {
            print_passed($test,"$! exited with value $exitcode");
            return;
        }
        print_failed($test, "$! died with signal $exitcode");
        print_log_matches('error','rsked',$log);
        $Deploy_errors += 1;
    }
}

#######################################################################

# Run rsked in test mode. If it returns 0 we are okay.
#
sub verify_cooling {
    my $test = "cooling";
    my $binar = "$HOME/bin/cooling";
    my $log = qx{$binar --test 2>&1};  # 1>/dev/null
    # Diagnose
    if ($? == -1) {
        print_failed($test, "failed to execute: $!");
        $Deploy_errors += 1;
    }
    elsif ($? & 127) {
        my $sigexit = ($? & 127);
        if ($sigexit != 0) {
            print_error($test, "$! died with signal $sigexit");
            $Deploy_errors += 1;
            return;
        }
    } else {
        my $exitcode = ($? >> 8);
        if ($exitcode == 0) {
            print_passed($test,"$! exited with value $exitcode");
            return;
        }
        print_failed($test, "$! died with signal $exitcode");
        print_log_matches('error',$test,$log);
        $Deploy_errors += 1;
    }
}

#######################################################################

print "$ProgVersion\n";

verify_no_rfkill();
verify_binaries();
verify_configs();
verify_xvfb();
verify_wifi();
verify_sound_card();
verify_amixer();
verify_hwclock();
verify_sdr();
verify_dirs();
verify_logs();
verify_nompd();
verify_space();
verify_rsked();
verify_cooling();
verify_autostart();
verify_crontab();
verify_gqrx();

print "$Deploy_errors Errors,  $Deploy_warnings Warnings\n";
exit($Deploy_errors + $Deploy_warnings);
