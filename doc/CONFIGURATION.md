# Configuration

1. [rsked](#rsked)
2. [schedule](#schedule)
3. [cooling](#cooling)
4. [mpd](#mpd)
5. [gqrx](#gqrx)
6. [check_inet](#check_inet)

The best approach to configuration is to begin with example configuration
files and modify them to suit your needs.

```
# On Raspberry Pi:
cp -a example-armv7l armv7l

# On 64-bit Intel Linux system:
cp -a example-x86_64 x86_64
```

The most common sorts of changes required are enabling or disabling
features, or altering path names to reflect where you have stored
files.  The `rsked` and `cooling` configuration files use the **JSON**
syntax (ECMA-404).  If you are unfamiliar with this, consult an
[introduction to JSON](https://www.json.org/json-en.html).

Pathnames in the JSON configuration
files for `rsked`, `cooling` and `schedule` may start with a tilde string
"~/" to indicate the `$HOME` directory of the user running `rsked`, as
with command line shells.  Other shell syntax is *not* supported, e.g.
wildcards.


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

- `schema` : identifies the format version for the configuration file

### General

- `application` : identifies the application targeted by this file
- `sched_path` : pathname of the schedule file
- `version` : the version of this configuration file

### Inet_checker

- `enabled` : if true, the internet monitoring feature is enabled
- `refresh` : interval between checks of internet status, seconds

The [inet checker](#check_inet) is a separate process that
periodically checks whether a usable internet connection is present.
If enabled here, `rsked` will use this status information to determine
whether internet streaming sources are viable.

### VU_monitor

- `enabled` : if true, the volume monitoring feature is enabled
- `vu_bin_path` : pathname of the VU monitor binary
- `timeout` : number of seconds of silence to infer audio source failure

The VU monitor is a child process that checks audio output at the
Linux sound system layer.  If no sound is emitted for timeout seconds,
`rsked` is notified.  If silence is unexpected, `rsked` will attempt
to restore programming, e.g. by switching to an alternate source.

### Mpd_player

- `enabled` : if true, the `mpd` player is enabled
- `debug` : if true, emit additional diagnostic logging from mpd player
- `run_mpd` : if true `rsked` runs `mpd` as a child process (recommended)
- `bin_path` : path to the `mpd` binary
- `socket` : pathname of the unix domain `mpd` control socket
- `host` : host on which `mpd` is running
- `port` : TCP port of the `mpd` control socket

It is best to let `rsked` run `mpd`--it can restart it if it dies or
becomes unresponsive.  The unix socket will be used to control `mpd` if
available, otherwise the TCP socket (host/port) will be used.

### Sdr_player

- `enabled` : if true, the SDR player (`gqrx`) is enabled
- `gqrx_bin_path` : pathname of the `gqrx` binary
- `gqrx_work` : pathname of working configuration file for `gqrx`
- `gqrx_gold` : pathname of the reference configuration file for `gqrx`
- `low_s` : signal strength (dbm) that is considered low
- `low_low_s` : signal strength (dbm) that is considered very low
- `device_vendor` : 4-char hex string for the USB SDR vendor
- `device_product` : 4-char hex string for the USB SDR product

The USB device vendor and product strings are used by `rsked` to determine if
a suitable SDR dongle is present on the bus. You can find these strings
by plugging in the SDR and running `lsusb`.

When a radio signal falls below the low low threshold `rsked` will
switch to an alternate source, e.g. another station or recorded music.


### Ogg_player

- `enabled` : if true, the `ogg123` player is enabled
- `device` : audio device used for playback
- `working_dir` : root directory for stored Music files
- `ogg_bin_path` : pathname of the `ogg123` binary

### Mp3_player

- `enabled` : if true, the `mpg321` player is enabled
- `device` : audio device used for playback
- `working_dir` : root directory for stored Music files
- `mp3_bin_path` : pathname of the `mpg321` binary


## Schedule

The schedule controls what `rsked` will play at any given time during
a week.  The configuration file is JSON, conventionally
`~/.config/rsked/schedule.json`. It is a single object
with these sections:

- *preamble* : meta-information in top-level string members
- *sources* : various audio sources
- *week map* : maps each day of the week to a day schedule
- *day programs* : times and programs for a particular day or class of days

### Preamble

The preamble members must include:

- `schema` : identifies the format for the configuration file;
  this must be the string `1.0` for the format described here.
- `version` : identifies in logs this particular revision of your
  schedule--change as needed for your reference.

### Sources

`sources` is a unique JSON object that names and defines all of the
audio resources that may be scheduled. Each source consists of a
*name* (the key) and a JSON object that has a number of required or
optional members describing its *attributes*.  Source names may be any
unique string. `rsked` distributes with a number of [stock sources](#Stock Sources)
with names starting with the "%" character. These are used for
`rsked` internal announcements.

#### Common attributes

Several attributes apply to more than one type of source:

- `mode` : one of `off`, `radio`, `ogg`, `mp3`, or `mp3_stream`
- `alternate` : the *name* of another source to be used if the source being
  defined is unavailable for any reason.  If no alternate is named,
  the alternate is `off`, in effect silence.
- `repeat` : whether to keep repeating the source for the scheduled
  duration (true/false).
- `dynamic` : whether the pathname should be computed at play time (true/false)

Dynamic resources have their resource location computed at the time of
of play by substituting certain symbols with components of the local date or time.
The substitution is described in `man 3 strftime`. For example, `%d` in the
string is replaced by the day of the month (01 to 31), and `%y` is the
year (e.g. 2020).

Source definitions must include exactly one of the keywords
`frequency`, `file`, `directory`, `playlist` or `url`.

#### Radio sources

- `mode` : `radio`
- `frequency` : string indicating frequency in MHz


#### Locally stored MP3 or OGG Files

- `mode` : `mp3` or `ogg`
- `file` : local pathname of the audio file

The pathname in `file` may be absolute or relative to the player
working directory. "~/" expansion will be performed on this pathname.

#### Playlists

- `playlist` : name of a playlist naming files of the type specified by `mode`
- `path` : pathname of the playlist

A playlist is a file that should have one media pathname per line.
The pathnames within the file should either be absolute, or relative to the
*working directory* defined for the player (`working_dir`) in `rsked.json`.
No shell expansion at all is performed on these pathnames. De facto standard
M3U file format is acceptable, but the M3U directives will be ignored.
Example playlists are included in the `mpd/playlists` directories;
replace these with playlists reflecting your stored audio files.
The simplest way to create playlists is with an `mpd` client like *Cantata*.

#### Network Streams

- `url` : a www URL for the mp3 resource, acceptable to the chosen player

Players `mpd` and `mpg321` will handle URLs ending in `.mp3`.
In theory ogg-vorbis streams could be handled too however we have been unable
to verify this.

#### Directories

- `directory` : a pathname of a directory with mp3 or ogg files as
  indicated by `mode`
  
Absolute or working-directory relative directories can be handled by
`mpg321` and `ogg123` players.


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

The distributed English language computer synthesized audio messages may be
replaced by any desired ogg files.

### Week Map

`weekmap` is a unique JSON *array* that specifies the names of day
schedules to play on each day of the week.  The first element of
the array specified the schedule for *Sunday* and the last element
specifies Saturday. Each element must be the name of a day
schedule object defined elsewhere in the file.

```
"weekmap" : [
     "sunday",
     "weekday", "weekday", "weekday", "weekday", "weekday",
     "saturday" ],
```

### Day Programs

All objects within unique top level object `dayprograms` are schedules for
particular days, or sets of days, as referenced by the week map.
If the same schedule should be followed Monday through Friday, one
can simply define a shared day program like "weekday":

```
"weekday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "07:30", "program" : "cms" },
        {"start" : "09:00", "announce" : "motd-ymd" },
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
begins. However at 09:00 an announcment "motd-ymd" will interrupt
"cms". Announcements are intended to be brief asides, e.g. a message
of the day.

Each day program must satisfy a few constraints:

1. Slot `start` must specify a starting time of day (HH:MM:SS, local
   time). The trailing seconds field is optional and is assumed to be
   "00" if absent.
2. Start times must be monotonically *increasing* with index into the array.
3. The first slot must be for time "00:00", and may *not* be an announcement.
4. Each slot must specify *either* a `program` *or* an announcement
   (`announce`) that names a defined *source*.


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
square brackets.  The `gqrx` application generates this file and that
is the easiest way to obtain additional examples if needed.

The most critical section for `rsked` is `[remote_control]`.
Configure it enable remote control and allow only the local host
to connect:

```
[remote_control]
allowed_hosts=::ffff:127.0.0.1
enabled=true
```

Also of interest is the `fft_rate` parameter in the `fft` section.
Set this to 0 to disable the waterfall display, saving many processor
cycles.

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
