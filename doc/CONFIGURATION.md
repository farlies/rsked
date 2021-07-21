# Configuration

1. [rsked](#rsked)
2. [schedule](#schedule)
3. [cooling](#cooling)
4. [mpd](#mpd)
5. [gqrx](#gqrx)
6. [check_inet](#check_inet)
7. [btremote](#btremote)

## Overview

The best approach to configuration is to begin with example
configuration files, copy and modify them to suit your needs.  If you
have *installed from a release tarball*, the example files are in their
required places but *must be renamed*: in each case, remove the
`example-` prefix from the name, e.g. `example-gold.conf` becomes
`gold.conf`.

The most common sorts of changes required are enabling or disabling
features, or altering path names to reflect where you have stored
files.  The `rsked` and `cooling` configuration files use the **JSON**
syntax (ECMA-404).  If you are unfamiliar with this, consult an
[introduction to JSON](https://www.json.org/json-en.html).
(Work on a web-based configurator for rsked schedules is in progress.)

Configuration changes you will almost certainly want to make include:

- `rsked/schedule.json` -- audio sources and times to play them
- `rsked/rsked.json` -- which players to use
- `mpd/mpd.conf` -- pathnames for music library, playlists, logs, db, socket
- `rsked/cooling.json` -- GPIO pin assignments matching your hardware


If you are *building from source*, see the section *Configuration Files*
in [INSTALLATION](INSTALLATION.md#configuration-files) for more
information. In overview:

- prepare a set of configuration files in `rsked/config/$MYCONFIG`
- compile `rsked` in the build directory
- use `ninja` to install the binaries and your configuration files
- make further adjustments to configuration files, do *either*
  - edit them in their installed locations, *or*
  - edit the ones in `rsked/config/$MYCONFIG`, and use `ninja install` to reinstall


### Pathnames in Configuration Files

Pathnames in the JSON configuration files for `rsked`, `cooling` and
`schedule` may start with a tilde string "~/" to indicate the `$HOME`
directory of the user running `rsked`, as with command line shells.
Other shell syntax is *not* supported, e.g.  wildcards.


## rsked

Command line options to `rsked` include:

```
--help                option information
--shmkey arg          shared memory key for status info
--config arg          use a particular config file
--console             echo log to console in addition to log file
--debug               show debug level messages in logs
--test                test configuration and exit without running
--version             print version identifier and exit without running
```


If no `--config` option is specified, `rsked` will read the file
`$HOME/.config/rsked/rsked.json`.

The `rsked.json` file governs that configuration of the `rsked`
application, *with the exception of* the [schedule](#schedule).
It consists of a single top-level object (dictionary) with a set
of member objects with string, boolean, or numeric members.

- `schema` : string, identifies format for the configuration file

### General

- `application` : string, identifies the application targeted by this file
- `sched_path` : string, pathname of the schedule file
- `version` : string, the version of this configuration file

The version string should allow rsked to detect a newer schedule
via lexicographical comparison.  A date string like "2020-09-23T14:41"
works well.

### Inet_checker

- `enabled` : boolean, if true, the internet monitoring feature is enabled
- `refresh` : number, interval between checks of internet status, seconds

The [inet checker](#check_inet) is a separate process that
periodically checks whether a usable internet connection is present.
If enabled here, `rsked` will use this status information to determine
whether internet streaming sources are viable.

### player\_preference

If different enabled players are able to play the same media/encoding
for a source, `rsked` will try them according to a preferred order
until one is found that works. The *default* order (`playermgr.cc`) is
currently:

1. Mpd_player
2. Ogg_player
3. Mp3_player
4. Vlc_player
5. Sdr_player
6. Nrsc5_player

Starting with schema 1.1, it is possible to change this order on a per
media/encoding basis.  In JSON, a `player_preference` object contains
nested media objects with encodings mapped to an array of player names

```
"player_preference" : {
    "radio" : {
        "wfm" : [ "Nrsc5_player", "Sdr_player" ]
    },
    "directory" : {
        "mp3" : [ "Mpd_player", "Mp3_player" ],
        "ogg" : [ "Ogg_player", "Mpd_player" ]
    },
    "file" : {
        "mp3" : [ "Mp3_player", "Mpd_player" ],
        "ogg" : [ "Ogg_player", "Mpd_player" ]
    }
}
```

It is only necessary to include entries that deviate from the
default preferences. This section is completely optional.

### VU_monitor

- `enabled` : boolean, if true, the volume monitoring feature is enabled
- `vu_bin_path` : string, pathname of the VU monitor binary
- `timeout` : number, seconds of silence to infer audio source failure

The VU monitor is a child process that checks audio output at the
Linux sound system layer.  If no sound is emitted for timeout seconds,
`rsked` is notified.  If silence is unexpected, `rsked` will attempt
to restore programming, e.g. by switching to an alternate source.

### Vlc_player

- `enabled` : boolean, if true, the `vlc` player is enabled
- `debug` : boolean, if true, `rsked` emits additional logging from the player
- `bin_path` : path to the `vlc` binary
- `wait_us` : integer, microseconds to wait for vlc to respond to commands

Stock VLC Media Player on most Linux distributions will play most
audio content. Starting with v1.0.5 it is experimentally available
for everything except FM radio.  It seems to work well on x86, but
has thrown odd errors on Arm (Debian 10). You might still prefer `mpd`
which has had far more testing with `rsked`. The `wait_us` might need to
be adjusted from its default (40,000 microseconds) if VLC proves sluggish
on the target embedded system.

### Mpd_player

- `enabled` : boolean, if true, the `mpd` player is enabled
- `debug` : boolean, if true, emit additional diagnostic logging from mpd player
- `run_mpd` : boolean, if true `rsked` runs `mpd` as a child process
- `bin_path` : path to the `mpd` binary
- `socket` : pathname of the unix domain `mpd` control socket
- `host` : string, host on which `mpd` is running, default localhost
- `port` : number, TCP port of the `mpd` control socket

Experience has shown it is best to let `rsked` run `mpd` as a child
process (`run_mpd=true`). This way, `rsked` can easily restart it if
it dies or becomes unresponsive. Note that default installations of
`mpd` will include *multiple* mechanisms to start `mpd` automatically at
login: these must be removed since multiple mpd instances will
conflict.

The unix `socket` will be used to control `mpd` if available,
otherwise the TCP socket (`host`/`port`) will be used.

### Nrsc5_player

- `enabled` : boolean, if true, the SDR player (`gqrx`) is enabled
- `gqrx_bin_path` : string, pathname of the `gqrx` binary
- `device_index` : integer, Gnuradio device index

This player is experimentally available, and plays HD radio.  The
source configuration does not yet offer any way to select alternative
channels, so HD1 is always selected (work in progress).  If HD is
available and the signal is strong, it will deliver noticably better
audio quality than the analog signal on `Sdr_player`. It also less CPU
power than the Sdr_player, possibly eliminating the need for
cooling. But two factors might lead you to *disable* this
player. First, it takes quite some time to start, resume, or to
change stations (on Arm, about 18 seconds) which may annoy some
listeners.  Second, it can be unlistenable if the station is weak or
suffers from interference; the analog signal seems to degrade more
gracefully.  Experiment in your listening location.

The `device_index` is normally 0 (the default) when only a single SDR
device is connected to the computer. If you have more than one, boy
you're fancy: you might need to specify which SDR `nrsc5` should use here.


### Sdr_player

- `enabled` : boolean, if true, the SDR player (`gqrx`) is enabled
- `gqrx_bin_path` : string, pathname of the `gqrx` binary
- `gqrx_work` : string, pathname of working configuration file for `gqrx`
- `gqrx_gold` : string, pathname of the reference configuration file for `gqrx`
- `low_s` : number, signal strength (dbm) that is considered low
- `low_low_s` : number, signal strength (dbm) that is considered very low
- `device_vendor` : string, 4-char hex of the USB SDR vendor
- `device_product` : string, 4-char hex of the USB SDR product

The USB device vendor and product strings are used by `rsked` to
determine if a suitable SDR dongle is present on the bus. You can find
these strings by plugging in the SDR and running `lsusb`. For example,
the Realtek RTL-2838 has entry: `0bda:2838`.  The `0bda` is the vendor
and `2838` is the product.

When a radio signal falls below the `low_low_s` threshold `rsked` will
switch to an alternate source, e.g. another station or recorded music.
Falling below the `low_s` threshold will generate a warning message in
the log.

### Ogg_player

- `enabled` : boolean, if true, the `ogg123` player is enabled
- `device` : string, audio device used for playback
- `ogg_bin_path` : string, pathname of the `ogg123` binary

The `device` string is any argument acceptable to the `-d` option of
ogg123; see the `man` page.

### Mp3_player

- `enabled` : boolean, if true, the `mpg321` player is enabled
- `device` : string, audio device used for playback
- `mp3_bin_path` : pathname of the `mpg321` binary

NOTE: the `device` attribute is not currently respected; the local
default device will be used.

## Schedule

The schedule controls what `rsked` will play at any given time during
a week.  The configuration file is JSON, conventionally
`~/.config/rsked/schedule.json`. It is a single object
with three main sections:

- *preamble* : meta-information in top-level string members
- *sources* : various audio sources
- *dayprograms* : times and programs for a particular day or class of days

NOTE: The pre-release `rsked` schedules used schema "1.0", which had
an additional section, the week map.  Only schemas "2.0" and later are
supported in rsked releases. Older schedules, (if any exist!) will
need conversion.  The new schema is structurally simpler and
facilitates the (under construction) a web-based graphical editor,
*rcal*.  Schedules can also easily be created and updated with a text
editor.

### Preamble

The preamble is a set of top level attributes. Four are required:

- `encoding` : string, must be `"UTF-8"`
- `schema` : string, identifies the format for the configuration file;
  this must be the *string* `"2.0"` for the format described here.
- `version` : string, identifies this particular revision of the schedule
- `description` : string, brief title for the schedule (used in the web editor)

The values of `version` should be lexicographically comparable so
newer schedules may be reliably recognized. The suggested format is an
ISO date time stamp like: `2020-09-23T14:41`. This string will appear
in `rsked` logs.

Additionally, the preamble may specify attributes indicating 
local directories to find audio files and directories:

- `library` : string, music library path
- `playlists` : string, playlist path
- `announcements` : string, announcement path

If specified, these must be **absolute** paths.
If unspecified, the `library` defaults to:  `"$HOME/Music"`,
the playlists defaults to `"$HOME/.config/mpd/playlists/"`, and
announcements defaults to `"$HOME/.config/rsked/"`.

### Sources

`sources` is a unique JSON object that names and defines all of the
audio resources that may be scheduled. Each source consists of a
*name* (the key) and a JSON object that has a number of required or
optional members describing its *attributes*.  Source names may be any
unique string. `rsked` distributes with a number of
[stock sources](#Stock Sources) with names starting with the "%"
character. These are used for `rsked` internal announcements; the
use of the "%" prefix for other source names is discouraged.

#### Common Source Attributes

Most attributes apply to more than one type of source.
Three attributes are **required** of every source:

- `medium` : string, one of `file`, `directory`, `playlist`, `stream`
- `encoding` : string, one of `ogg`, `mp3`, `mp4`, `flac`, `wfm`, `nfm`, `mixed`
- `location` : a pathname, frequency, or URL (see media sections below)

The encoding indicates how the audio is represented in the medium.
Values correspond to well-known file and broadcast types.

Several more attributes are common, but have default values:

- `alternate` : string, the *name* of another source
- `repeat` : boolean, whether to keep repeating the source
- `quiet` : boolean, whether it is okay for the source to be silent
- `dynamic` : boolean, whether the pathname should be computed at play time
- `announcement` : boolean, is the source an announcement
- `duration` : number, seconds to play

The `alternate` is a source to be played if the source being
defined is unavailable for any reason.  If no `alternate` is named,
the default alternate is `off`, a built-in source that is silent.
  
If `repeat` is `true` then the source will be started again as
many times an necessary to fill the scheduled period. The default
is `false`.  Setting it to `true` might be sensible for a locally
stored recording.

A `quiet` source may be silent for extended periods without `rsked`
flagging a playback problem. It is `false` by default, except for
local media that do not have the `repeat` option set to `true`.

`dynamic` resources have their actual resource location computed at the
time of of play by substituting certain symbols with components of the
local date or time.  The substitution is described in `man 3
strftime`. For example, `%d` in the string is replaced by the day of
the month (01 to 31), and `%y` is the year (e.g. 2020).
This is handy to fetch internet resources that are date-named or
files to be played on particular days.

`announcement` sources are intended to be *briefly* interrupt normal
programs, which resume when the announcement finishes. Default: `false`.
NOTE: currently announcements must use **ogg** encoding.

`duration` indicates the number of seconds (and fractions thereof) that
are required for a recorded work to play to completion. Omit if not known.
It is currently used only the `rcal` schedule editor.


#### Radio Sources

- `medium` : string, `radio`
- `encoding` : string, either `wfm` or `nfm`
- `location` : double, indicating frequency in MHz

Standard FM broadcast stations use "wide" FM (wfm) and a frequency
that is a multiple of 100KHz, e.g. `90.3`. "Narrow" FM is used for
other stations such as weather and emergency services; it is 
currently *not* supported by the SDRplayer, but will be in the near
future.

#### Locally Stored Music Files

- `medium` : string, `file` or `directory`
- `encoding` : string, `ogg`, `mp3`, `mp4`, or `flac`
- `location` : string, local relative pathname of the audio file

Files and directories are taken to be relative to the
*music library path*. For example:  `"brian eno/another green world"`
would be: `"/home/pi/Music/brian eno/another green world"`
in a default configuration for user `pi`.

There is one exception: *announcements* are taken to be relative to
the announcement directory, e.g. `"motd/fcookie.ogg"`
might be translated to: `"/home/pi/.config/rsked/motd/fcookie.ogg"`


#### Playlists

- `medium` : `playlist`
- `location` : string, pathname of the playlist

The `location` filename, e.g. `foo.m3u`, is taken to be relative to
the *playlist directory*.  A playlist is a file that should have one
media pathname per line.  No shell expansion at all is performed on
these pathnames. De facto standard M3U file format is acceptable, but
the M3U directives will be ignored.  Example playlists are included in
the `mpd/playlists` directories; replace these with playlists
reflecting your stored audio files.

There are some differences in the way different media players interpret the
pathnames in playlists.  However in general, relative pathnames or
bare filenames are assumed to be relative to the directory containing
the *playlist* file.  For best interoperability you may either (or
both) of:

1. Put all of your playlists at the root of the music directory (so
   relative paths will coincide with the assumed root), or make
   symbolic links so that they appear in all required places.
2. Only use *full pathnames* in playlists.  This option is arguably
   best long term as it should be unambiguous to any current or future
   player.



#### Network Streams

- `medium` : `stream`
- `encoding` : string, `mp3`, `ogg`, `mp4`, or `flac`
- `location` : string, a www URL for an audio resource

Players `mpd` and `mpg321` will handle URLs ending in `.mp3`.
In theory, the other encodings could be handled too (by `mpd`),
but this has received no testing as of yet.


#### Stock Sources

`rsked` is distributed with a number of Ogg source files that are used
(if available) as announcements. These are installed in the directory
`$HOME/.config/rsked/resource/`.

- `%snooze1` : snooze for one hour
- `%resume` : resuming normal program
- `%revert` : reverting to alternate source
- `%goodam` : good morning
- `%goodaf`  : good afternoon
- `%goodev`  : good evening
- `%motd` : message of the day

The distributed English language (computer synthesized) audio messages
may be replaced by any desired **ogg** files.


### Day Programs

All objects within unique top level object `dayprograms` are schedules
for particular days. There must be exactly 7 of them, with names:
"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", and
"saturday". They may appear in any order in the JSON.

```
"monday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "07:30", "program" : "cms" },
        {"start" : "09:00", "announce" : "motd-md" },
        {"start" : "14:00", "program" : "nis" },
        {"start" : "15:00", "program" : "cms" },
        {"start" : "15:30", "announce" : "motd-ymd" },
        {"start" : "21:00", "program" : "OFF" }
],
```

The schedule consists of a non-empty *array* of *slots*. There are two
types of slots: *programs* and *announcements*.  Programs are sources
that should be played starting at the given start time until the next
program start time. Announcements are sources that interrupt programs
at specified start times--the regularly scheduled program is resumed
when an announcement finishes. In the example above, the source "cms"
will commence playing at 07:30 and play until 14:00, when source "nis"
begins. However at 09:00 an announcement "motd-ymd" will interrupt
"cms". Announcements are intended to be brief asides, e.g. a message
of the day.

Each day program must satisfy a few constraints:

1. Slot `start` must specify a starting time of day (HH:MM:SS, local
   time) using a 24-hour clock format, e.g. "14:30:00" would be
   2:30PM. The seconds field (:SS) is optional and is assumed to be
   ":00" if absent.
2. Start times must be monotonically *increasing* with index into the array.
3. The first slot must be for time `"00:00"`, and may *not* be an announcement.
4. Each slot must specify *either* a `program` *or* an announcement
   (`announce`) whose value names a *source* defined in the schedule
   or the special source `"OFF"`
5. There must be at least one slot.


### Schedule Validation

A JSON schema for the schedule is in `scripts/schedule_schema.json`
and may be used to validate a schedule with your favorite validator,
e.g. https://github.com/neilpa/yajsv

```
$ yajsv -s scripts/sked_schema-2.0.json ~/.config/rsked/schedule.json 
/home/qrhacker/.config/rsked/schedule.json: pass

```

The schema check covers syntax and basic semantics. The test mode of
`rsked` (run with `--test`) does functional validation of settings.


## cooling

`cooling` is an application that may be used on embedded installations
to control both rsked and several GPIO peripherals: fan, button, LEDs.

Command line options for `cooling`:
```
  --help                option information
  --config arg          use a particular config file
  --console             echo log to console in addition to log file
  --debug               show debug level messages in logs
  --test                test configuration and exit without running
  --version             print version identifier and exit without running
```

The `cooling.json` file governs that configuration of the `cooling`
application.  Like `rsked.json`, it consists of several JSON objects
with members specifying runtime options.

- `schema` : identifies the format version for the configuration file;
  this should be "1.1" for the version of `cooling` described in this
  document

The `--test` command line option is useful to validate a configuration
file. It will print the effective settings and exit.
  

### General

The section `General` governs polling: how frequently the application
checks the objects it is configured to monitor. If cooling is supposed to
check for button presses, this should be 250 milliseconds or faster; otherwise
a 1-second interval should suffice.

- `application` : identifies the application targeted by this file: cooling
- `version` : the version of this configuration file
- `poll_msec` : interval at which cooling performs checks, milliseconds 
- `poll_trace` : count of polls after which a trace message will appear in logs

### Rsked

The section `Rsked` determines whether and how `cooling` runs `rsked`
as a child process. The advantage of this arrangement is that
`cooling` can monitor the status of `rsked` and restart it cleanly if
it should ever exit abnormally.

- `enabled` : whether `cooling` should run `rsked` (true/false)
- `rsked_bin_path` : pathname of the `rsked` binary
- `rsked_config_path` : pathname of the `rsked` configuration file
- `rsked_debug` : whether to run rsked with its debug flag (true/false)
- `kill_pattern` : pattern suitable for `pkill` used to kill any rogue players
- `mpd_stop_cmd` : command used to stop any `mpd` player

If `cooling` starts or restarts `rsked`, it will first attempt to stop and kill
any audio players using the kill pattern and `mpd` stop command, if defined.
This prevents multiple players from running if for some reason `rsked` failed
to stop them on exit.

### Cooling

This section determines whether and how `cooling` will operate a
cooling fan. This feature is useful on some hardware when running SDR,
which may cause the CPU temperature to spike.
See the [README-RPi](README-RPi.md) for more hardware details.

- `enabled` : whether `cooling` should control an external fan (true/false)
- `fan_gpio` : GPIO pin number of the fan
- `sensor` : pathname of the thermal sensor special file
- `cool_start_temp` : temperature (C) to turn the fan ON
- `cool_stop_temp` : temperature (C) to turn the fan OFF
- `min_cool_secs` : minimum time to run the fan (seconds)

### SnoozeButton

One feature of `rsked` is that can suspend play ("snooze") for period (1 hour).
It does this when it receives the signal `SIGUSR1`. If enabled, `cooling`
will monitor a GPIO pin, typically connected to a pushbutton, and
send this signal to `rsked` each time the pin transitions from high
to low then back to high.

- `enabled` : whether `cooling` should monitor a "snooze" button
- `button_gpio` : GPIO pin number of the button (normally high)


### PanelLEDs

If `cooling` runs `rsked`, it receives regular status updates.
`rsked` status can be displayed on two external LEDs.  The *green*
LED is ON when `rsked` is playing normally; it is OFF when `rsked`
is in the OFF mode (silent); it blinks when `rsked` is temporarily
paused (snooze).  The *red* LED is lit when an error is detected
that prevents `rsked` from running.

- `enabled` : whether `cooling` should control external indicator LEDs
- `green_gpio` : GPIO pin number of the a green status LED
- `red_gpio` : GPIO pin number of the a red status LED


## mpd

The Music Player Daemon, `mpd`, has a configuration file that is
documented in the
[mpd user manual](https://www.musicpd.org/doc/html/user.html) and also
in the file's internal comments.  The preferred way to run `mpd` is
to allow `rsked` to start it with a a user-specific configuration
file, conventionally `.config/mpd/mpd.conf`.

The following parameters may need to be changed to reference the user
home directory of the target user.

- `music_directory`
- `playlist_directory`
- `db_file`
- `log_file`
- `pid_file`
- `state_file`
- `sticker_file`
- `bind_to_address`

Also, certain parameters are atypical for default mpd installations:

- `user` : comment this out
- `restore_paused` : set to `yes`
- `auto_update` : set to `no`

Under the `audio_output` section, the stanza for type `alsa` may need
to be adjusted for your audio device. E.g. for a USB audio dongle on
your RPi, you might use:

```
audio_output {
	type		"alsa"
	name		"USB Audio"
	mixer_type	"hardware"
	mixer_control	"Master"
}
```

Other audio output options should work.  Test the configuration outside
of `rsked` to verify it works with your hardware.

The example `mpd/` configuration directory includes a subdirectory
`playlists/`.  Any **playlist** files here will be installed into the
`mpd` configuration playlist directory as a side effect of `ninja install`.


## gqrx

`gqrx` uses an "ini" file syntax, with sections denoted by names in
square brackets.  The `gqrx` application generates this file, and
running it is the best way to obtain a usable configuration.  The
parameters values may *vary considerably* based on the SDR hardware
you have attached.  Run `gqrx` from the desktop and make adjustments
as needed, e.g. selecting the right SDR hardware. Pay particular
attention to the audo window **gain** slider: the default value may be
far too high for powerful commercial FM stations.  Keep the gqrx
window small, and in the top left corner of the screen.  Disable
dynamic features for embedded use. Save the configuration file,
e.g. as `gqrx.conf` and exit. Copy the saved configuration file to
`gold.conf`, which is the configuration file that will be used by
`rsked`.

Edit the file `gold.conf` to adjust or verify certain settings:

1. Under `[General]`, verify that parameter `crashed=false`.
2. Set the`fft_rate` parameter in the `[fft]` section to 0
   to disable the waterfall display (for embedded use).
3. Under the section `[remote_control]`, configure it enable remote
   control, and allow only the local host to connect, e.g.

```
[remote_control]
allowed_hosts=::ffff:127.0.0.1
enabled=true
```
Note that the working copy of `gold.conf` will be overwritten each time
`ninja install` is invoked from the `rsked` build directory. Consider
installing your gold.conf in the build's `config/<arch>/gqrx/`
subdirectory.

## check_inet

This section is relevant only to users that intend to include internet
stream sources in their schedule.  The `check_inet.sh` helper
application is a `bash` shell script that verifies internet usability.
It performs its checks by attempting to retrieve a small file with a
web utility (`wget`) from relevant internet servers.  It is intended
to be run periodically, e.g. by user `cron`, and writes a status file
`netstat` in the user's XDG runtime directory, e.g.:

```
0 2020-07-21 18:11:17-05:00 GOOD network
```

Several bash variables configure `check_inet.sh`. Most of them may be
left to their in-script defaults, however some users may need or wish
to modify them, e.g. to specify the correct network interface. To do this,
add (or modify) the file `$HOME/.config/rsked/ci.conf` to include new
definitions for the selected variables. The examples include a
`ci.conf`. The source for `check_inet.sh` contains additional
documentation.

- `IFACE` : set this to the network interface that will be used to
  access the internet. If Wi-Fi is being used, this might be set via
  `IFACE=wlan0`. Aside: `wlan0` is actually the default. However if you are
  using ethernet, you **will** need to explicitly set it to the ethernet
  adaptor, e.g. `IFACE=enp3s0`. You can learn the names of the adaptors
  on your host with the `ip link` command.
- `RHOST` : internet host name used to test the configured domain name
  service, e.g. `RHOST=example.net`
- `LOGFILE` : pathname of a file to write log messages if problems occur; 
  by default this is `$HOME/logs/check_inet.log`
- `URLS` : a list of URLs to test


```
URLS=("https://www.example1.com/robots.txt"
      "https://www.example2.com/robots.txt"
      "https://www.example3.com/robots.txt")
```

The URLs are tested in order until one is found to work or all fail.
The next time `check_inet.sh` is run, it will test the next URL in the
list. It is best to select URLs that are very small files (robots.txt
is *usually* small), and hosts that are reliably "up" and responsive.
Never use a URL for a stream.

## btremote

The easiest way to interact with the embedded application is usually
via IP networking--WiFi or Ethernet allows commands to be issued
easily via SSH or web service.  However there are some environments in
which IP networking is impractical, not permitted, or cannot be
configured in advance.  Bluetooth is often a viable solution in these
cases.  The optional `btremote` service offers a way to interact with
the embedded `rsked` device from a phone or tablet to check its status
and configure some parameters.  It uses Bluetooth as a serial port
service on the device, and a terminal emulator on the phone is used to
issue brief commands.


### Interacting with btremote

The `btremote` command may be used in the field to check on 
the status of the `rsked` device and to configure certain
parameters. Commands include:

```
  help   : print this list of commands
  boot   : reboot rsked device now
  halt   : power off rsked device now
  last <lg> [<n>] : tail logfile
  ls     : log dir list
  quit   : end session
  status : print the network status
  time [<date>] <time>  : get/set time
  warn <lg> [<n>] : tail warnings
  wadd <ssid> <psk>  : add WiFi net
  watt    : attach WiFi
  wls     : list stored WiFi nets
  wrm <j> : remove stored WiFi net
  wscan   : scan visible WiFi nets
```

### Configuring WiFi

To configure WiFi via `btremote` first run the `wscan` command.  This
will print a list of discovered SSIDs. You can use the index number
from this list with the `wadd` command; if your network does not
appear here, use its literal SSID instead.  (Surround the SSID or
preshared key with double quotes if it has spaces).

```
Sat Nov  7 15:10:22 2020
rsked> wscan
Starting scan...
1 ESSIDs stored
Successful Wi-Fi scan
 1. gatornet, ncryptd, 49/70

rsked> wadd 1 MySecretWPAKey
rsked> watt
```

### Setting the Time

If the `rsked` device cannot use NTP, then the clock will
inevitably drift. The `btremote` command `time` may be used
to manually set a time (and date). This will set both the
system time and the battery RTC. Time should be specified
in 24-hour format and should be the *local* time.
Examples:

```
rsked> time
 2020-11-11 09:07:32

rsked> time 09:09:01
rsked> time 2020-11-12 15:22:00
```

### Checking Logs

The `ls` command will list the current logs.
The `last` command will print the latest lines from a given log file
given its name (or a prefix thereof). The `warn` command will
print only errors and warnings from a log.

```
rsked> ls
total 204K
 60K cooling_00030.log
 32K rsked_00032.log
 56K clock.log
4.0K vumonitor_00030.log
 12K rsked201015_131929.out
 32K mpd.log
4.0K check_inet.log
4.0K rsked201009_093130.out

rsked> last cool 4
2020-11-07 15:30:38 <info> [cooling] rsked Playing, temperature 51.54 C
2020-11-07 15:31:23 <info> [cooling] Halted FAN at temp=49.926
2020-11-07 15:31:53 <info> [cooling] rsked Playing, temperature 53.692 C
2020-11-07 15:33:08 <info> [cooling] rsked Playing, temperature 57.996 C

rsked> warn vumon
2020-11-07 11:15:24 <warning> [vumonitor] TOO QUIET check #7191
2020-11-07 14:49:40 <warning> [vumonitor] TOO QUIET check #7294
```
