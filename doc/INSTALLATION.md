# Overview

1. Configure target hardware and operating system
2. Install prerequisite tools and libraries
3. Install and test external source players on the target system
4. Download sources and compile `rsked` or download a release archive
5. Create configuration files for your application (templates provided)
6. Install `rsked` binaries and configuration files
7. Install start-up script to run on boot (optional)
8. Install a crontab to perform any periodic maintenance tasks (optional)
9. Enable bluetooth if using btmonitor service (optional)

If you are building on Raspberry Pi, be sure to read
[README-RPi](README-RPi.md) too.

# Operating System

`rsked` and associated players should run on most (vintage 2020) Linux
systems, but we give details for two platforms regularly tested and
known to work:

- Ubuntu 20.04  on x86_64
- Raspbian 10 (Buster) on Raspberry Pi 3B+

# Release

## Dependencies

To run a binary ARM release begin with at least a minimal desktop
installation of Debian 10 Buster (Raspbian).  The following additional
deb packages are required for a typical installation:
- vorbis-tools
- mpd, mpc
- mpg321
- pulseaudio
- dnsutils
- gnuradio, libgnuradio-osmosdr0.1.4, libvolk1-bin
- libboost-program-options1.67.0
- libboost-log1.67.0
- libboost-regex1.67.0
- libgpiod2
- libjsoncpp1

### Notes

- On Ubuntu 20.04, replace `boost1.67` packages with `boost1.71`.

- The VLC Media player is typically included in the minimal install, but
  if command `vlc` is missing, install `vlc`.

- You may omit the gnuradio packages if there is no need to play FM
  radio via `gqrx` (and the SDR player is disabled in the configuration.)

- You may omit `mpd`, `mpc`, and `mpg321` if there is no need to play
  MP3 files or streams with these players. (Be sure to disable them in
  the configuration).

- You may omit `dnsutils` if the application will not use the IP network
  at all, or will not need to run `check_inet`.


## Installing a Binary Release

Download a release and signature from GitHub. Verify integrity using the 
signature and key for farlies@gmail.com available from https://keys.openpgp.org
with fingerprint:

```
2B0B 435B 1522 A8ED 2E54  E44E 4B14 253F 7681 B2A2
```

The `tgz` file is designed to be expanded into the *home directory* of
the user that will run `rsked`, e.g. "pi".  **Note well** that
depending on options given to `tar`, unpacking the archive might
overwrite (or fail to overwrite) any identically named files from a
previous installation in `~/bin` or `~/.config/{rsked,gqrx,mpd}`. If
you wish to preserve such files, e.g. an existing schedule, *move
them* to a safe place before expanding.

```
cd
tar xzf ~/Downloads/rsked1.0.3-armv7l-release.tgz
```

Replace the pathname above with one corresponding to whatever release
you downloaded.

Edit the files in the `~/.config` directories per instructions in
[CONFIGURATION](CONFIGURATION.md).


# Build Tools and Libraries

To build the `rsked` applications (C++ programs) from source, you will
additionally require the following development tools and libraries:

- gcc    : C++ compiler suitable for C++ 17 (clang will work too)
- meson  : build system
- ninja  :  build tool
- boost  : version at least 1.67, tested up to 1.71
- libjsoncpp-dev
- libpulse-dev
- libusb-dev


##  Ubuntu 20.04:

```
sudo apt-get update
sudo apt install build-essential gcc
sudo apt install git meson
sudo apt install libboost1.71-dev libboost-system1.71-dev
sudo apt install libboost-log1.71-dev libboost-program-options1.71-dev
sudo apt install libboost-test1.71-dev libboost-regex1.71-dev
sudo apt install libjsoncpp-dev
sudo apt install libpulse-dev
sudo apt install libgpiod-dev
sudo apt install libusb-1.0-0-dev
sudo apt install libmpdclient-dev
```

The particular version of `libboost` may be different (not 1.71)
on your system--check what is available and substitute accordingly.

## Raspbian Buster

```
sudo apt-get update
sudo apt install build-essential gcc
sudo apt install git meson
sudo apt install libboost1.67-dev libboost-system1.67-dev
sudo apt install libboost-log1.67-dev libboost-program-options1.67-dev
sudo apt install libboost-test1.67-dev  libboost-regex1.67-dev
sudo apt install libjsoncpp-dev
sudo apt install libpulse-dev
sudo apt install libgpiod-dev
sudo apt install libusb-1.0-0-dev
sudo apt install libmpdclient-dev
```

# Player Installation

## ogg123

This player is currently required since it is used for miscellaneous
audio announcements. 

```
sudo apt install vorbis-tools
```

## vlc

The VLC Media player is commonly pre-installed, but may be installed various ways
if needed, e.g.:

```
sudo apt install vlc
```

## mpd

The *Music Player Daemon* is an alternative to VLC in that it also plays a wide
variety of audio files and streams.

```
sudo apt install mpd mpc
#
sudo systemctl disable mpd.service mpd.socket
systemctl --user disable mpd.service mpd.socket
sudo rm /etc/systemd/user/default.target.wants/mpd.service
sudo rm /etc/systemd/user/sockets.target.wants/mpd.socket
sudo rm /etc/xdg/autostart/mpd.desktop
rm ~/.config/autostart/mpd.desktop
```

NOTICE: Conventionally `mpd` is started at boot or at login by
`systemctl` or the desktop session. *However*, it is best to let
`rsked` start it as a child process. The standard package installation
*will* attempt to start `mpd` (in several ways!) so this must
be prevented via the trailing commands above. Also, check, from the target
user's Ubuntu Gnome launchpad, search for "Startup Application
Preferences" and run this program; within it, **disable** any startup
checkbox for `mpd`. After a reboot check the startup logs to make
sure it is not still starting:

```
journalctl --unit=mpd
journalctl --user --unit=mpd
```

## mpg321

Optional. Not needed if using `mpd` or `vlc`, but it has a much smaller
footprint.  Note that it will only play `mp3`, not `mp4/m4a` files.

```
sudo apt install mpg321
```

## nrsc5

Optional. This program receives NRSC-5 digital radio stations ("HD")
using an RTL-SDR dongle. It offers a command-line interface as well as
an API upon which other applications can be built. `rsked` drives the
command line application.  Retrieve the source from GitHub, then build
and install per directions.

- https://github.com/theori-io/NRSC5


## gqrx

Optional. If you wish to use the `gqrx` player for FM radio, this
currently must be built from a particular fork of the source. Stock
`gqrx` from a Linux distro's package will *not* have the remote
protocol extensions needed by `rsked`.  Building `gqrx` entails a
number of additional tools and libraries.  Consult
[gqrx](https://github.com/farlies/gqrx) for details.


# Compiling rsked

Fetch sources:

```
git clone https://github.com/farlies/rsked.git
cd rsked
```

The `setup.sh` script configures a build directory for a target architecture 
(`armv7l` for RPi or `x86_64` for Intel/AMD) and build type (`release` or `debug`).
We refer this directory as `MYBUILDDIR` hereafter. With no arguments,
it will produce a *debug* build for the *native* architecture using `gcc`.

```
./setup.sh
cd debug-x86_64
ninja
```

Options to `setup.sh` can produce different configurations.
For example, a release (`-r`) build with the clang (`-c`) compiler
and a build directory named `mybuild`

```
./setup.sh -rc mybuild
cd ./mybuild
ninja
```

Note: If compiling on the RPi, adding the argument `-j1` to `ninja`
may be more efficient.  Cross compilation (e.g. build ARM binaries on
x86_64) is not currently supported (TODO).


## Logs

Before attempting to start anything, create log directories on the
target system:

```
mkdir -p $HOME/logs  $HOME/logs_old
```

The `logs/` directory contains the logs currently being written.
The `logs_old/` directory contains a limited set of older logs.


# Configuration Files

There are two *examples* of configuration directories, one for each
architecture:

- `config/example-x86_64/`  x86 workstation oriented (no gpio)
- `config/example-armv7l/`  Raspberry Pi oriented (gpio enabled)

Copy an example configuration directory to a directory matching the
desired architecture.  For example, from `$MYBUILDDIR`:

```
cd ../config

# From inside the 'config/' directory:

cp -a example-x86_64 ./x86_64
cd ./x86_64
```

Edit the files in the new directory per instructions in
[CONFIGURATION](CONFIGURATION.md).

# Install

After the configuration files have been prepared, return
to the build directory and use `ninja` to install all files.
`rsked` is meant to be run as an ordinary user and the working
files all reside in the `$HOME` directory of the installing user.

```
cd $MYBUILDDIR
ninja install
```

Binary files will be installed to `$HOME/bin`. 
Add this directory to your `PATH` environment variable if necessary.

Configuration files will be installed in `$HOME/.config/{rsked,mpd,gqrx}/`.

>NOTE: after installation, you may edit the files in their installed locations.
>Note however that each "ninja install" will overwrite the installed
>files with ones from the associated `config` directory. So, if you
>expect to do multiple installations (e.g. you are modifying rsked
>sources), it is better to edit configuration files in the "rsked/config"
>directory and use ninja to install them from there.

Some further one-time steps must be done at the command line.
The `ninja install` step will *not*:

- install a `crontab`
- make hardware specific changes as described in [README-RPi](README-RPi.md)
- configure `rsked` to autostart on login


## Runtime Directory

Although `rsked` is designed to run *without* a graphical user interface,
it does need a place for certain temporary files. It looks
for this in the user's XDG runtime directory, as identified
by environment variable `XDG_RUNTIME_DIR`.  Verify that this
variable has been set when the embedded user (e.g. `pi`) logs in:

```
echo $XDG_RUNTIME_DIR
```

# Startup

## Testing

For testing, one can just run `rsked` from the command line, or run
`cooling` and let it start `rsked`. Check that log files appear in
the `$HOME/log` directory.

There is also a *test mode* so that configuration may be tested
*without* operationally running the programs. Start the programs
individually with the option `--test`:

```
~/bin/rsked --test
~/bin/cooling --test
```

They will check their configurations and exit (in a few seconds).  The
log will appear on the console: scan it for any errors or warnings.
Like other Linux programs, the program exit code will be 0 if no fatal
problems were detected.

## Embedded

In the embedded environment, the application should start automatically
after boot since there is typically no user interface.
The installed script `startup_rsked` will do this.
See the Startup section in [README-RPi](README-RPi.md)


# Cron

The Linux `cron` daemon (`man 1 crontab`) can be used on an embedded device to:

- run the `check_inet.sh` script
- upload logs to a cloud host

An example `crontab` file is included that runs check_inet every 5 minutes
between 6 AM and 9 PM, and synchronizes logs with a remote `LOGHOST`
3 times every day:

```
LOGHOST=loghost
#
#min      hr   dom   mon   dow   cmd
0-59/5   6-21    *     *     *    $HOME/bin/check_inet.sh
1     7,14,22    *     *     *    $HOME/bin/synclogs.sh $LOGHOST >$HOME/logs/synclogs.out 2>&1
```

# Bluetooth

If you wish to use the optional Bluetooth monitor service to
interact with the embedded device via a phone or tablet, some
additional setup is required.

## Enable bluetooth

Unblock bluetooth if needed:

    rfkill unblock bluetooth

Enable the *serial port protocol* in bluez. There are some online
guides for this, but in brief:

Edit as root the Bluetooth desktop service configuration file:
`/etc/systemd/system/dbus-org.bluez.service`

Look for the line that starts with `ExecStart`; add the compatibility flag
"-C" at the end of the command like:

    ExecStart=/usr/lib/bluetooth/bluetoothd -C

In the same file *add* a new line immediately below the `ExecStart`
line to add the serial protocol:
	
    ExecStartPost=/usr/bin/sdptool add SP

Save the file, then reload the configuration file and restart the
service:
	
    sudo systemctl daemon-reload
    sudo systemctl restart bluetooth.service

## Install the btmonitor Service

Install the service file and enable the service:

    sudo cp ~/bin/btremote.service /usr/lib/systemd/system/btremote.service
    sudo systemctl enable btremote.service
    sudo systemctl start btremote.service
    journalctl _SYSTEMD_UNIT=btremote.service

## Mobile Device Preparation

Install software on your mobile device that can send and receive over
the Bluetooth serial protocol.  `btremote` has been tested with the
Android app *Serial Bluetooth Terminal* (1.33 or later) by Kai Morich,
available on the Google Play Store.  Conveniently, this app has a row
of buttons that can be programmed with btremote commands.  Set the
display mode to "Terminal", and the font size to the smallest readable
one. The line termination for sending should be set to use just the CR
character.
    
## Pair Your Mobile Device

*Before* deploying the embedded `rsked`, pair it with your mobile
device(s).  The desktop application on Debian or Ubuntu should do the
trick, however it is also easily accomplished with the `bluetoothctl`
utility.

     $ bluetoothctl
     agent on
     discoverable on
     scan on
     ...
     quit
 
 Find the embedded device in your mobile device's menu of discovered
 stations.  Answer any pairing challenge on either side (while in
 `bluetoothctl`).
 
