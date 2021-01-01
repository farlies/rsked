rsked
=====

`rsked`, short for *radio scheduler*, is a Linux application that
plays any of various audio sources at specific times on a weekly
schedule, without any requirement for human interaction.  Sources
include locally stored recordings (ogg, mp3, mp4, flac), internet
streams, and FM broadcasts (via inexpensive SDR hardware). It is
intended to be run as an **embedded application** on an inexpensive
low power microsystem (Raspberry Pi 3, RPi).  However it can run on
x86 desktop computers as well, and likely on any Linux computer with a
sound card.

`rsked` was originally developed for people who, due to disability,
are physically unable to operate conventional audio equipment. It
satisfies a desire to have a variety of audio programming without
reliance on others to control the equipment. It can work in an
environment where internet connectivity is absent or unreliable. It
could be used in any situation where it is inconvenient or undesirable
to require human interaction to schedule audio programming.

## Features

- Weekly program schedule defined in a simple JSON file
- Play MP3, MP4, FLAC, or Ogg-Vorbis files, directories, or playlists
- Play internet streams
- Play FM broadcast stations
- Audio "message of the day" at scheduled times
- Audio source failure detected, with reversion to defined backup sources
- Runs on Raspberry Pi-3 or x86_64, no screen required
- Snooze button to pause or resume play (optional, GPIO)
- Status LEDs (optional, GPIO)
- Control of active cooling (optional, GPIO)
- Logging of operations for remote monitoring
- Bluetooth status monitoring and setup (optional)

See [CHANGES](doc/CHANGES.md) for a summary of important changes that have
occurred over versions of `rsked`.


## Usage Overview

The overall system consists of several applications compiled from this
repository, and a selection of external audio players that you must install
separately.  The choice of external players depends on the sources
desired for the intended application.

![architecture]

[architecture]: doc/architecture.png

A typical embedded application will:

1. Compile and install the `rsked` software and external audio players
2. Adjust configuration parameters to select the desired features
3. Install any desired recorded music files
4. Configure a *schedule* that references local and remote music sources
5. Set `rsked` to start automatically when the embedded device boots.


### Recorded Music

Recorded music may be played with any of these external applications
available as packages on most Linux distributions:

- [ogg123](https://wiki.xiph.org/Vorbis-tools)
- [vlc](https://www.videolan.org)
- [mpd](https://www.musicpd.org)
- [mpg321](http://mpg321.sourceforge.net/)

The `ogg123` player plays Ogg-Vorbis recordings, and is required to
play announcements in `rsked`. `vlc` plays a wide range of audio file
formats.  The music player daemon `mpd` is similarly versatile.  The
lightweight `mpg321` player is suitable for MP3 files.  All of these
programs can play both individual files and playlists.


### Streaming

All of the above mentioned applications also handle streaming sources,
such as internet radio stations. Streaming connections occasionally
*break* for reasons outside of one's control.  `rsked` can detect and
respond to such problems.  No player I've tested handles the full
range of failures encountered in the field, but *mpd* seems most
robust.  `ogg123` can support ogg vorbis streams, although
these have not been tested in `rsked`.

### FM Radio

Over-the-air FM broadcasts may be scheduled using *Software Defined
Radio* (SDR). Attach an inexpensive USB SDR radio dongle such as
[RTL-2832U](https://www.rtl-sdr.com/), then configure the
desired frequencies in the schedule.  `rsked` uses a slightly modified
fork of [gqrx](https://github.com/farlies/gqrx). Consult that project
for further instructions.

### rsked application

The `rsked` application reads a configuration file and a weekly
schedule. It then orchestrates external players to deliver the
requested audio content at the correct times. `rsked` should be
started after the target system boots. This job may be performed by
the `cooling` application described below.  `rsked` is the sole
mandatory application in the system.

### cooling application

The `cooling` application is a supervisor for `rsked`.
`cooling` should be started after the target system boots.
It is not essential, but is recommended for embedded applications.
It can be configured to:

- start or stop a cooling *fan* based on CPU temperature
- start rsked with desired options
- restart rsked if it stops for any reason
- signal rsked that a button has been pressed
- control external LEDs to indicate status

RPi systems may need active cooling (fan) if the SDR player is used
extensively. SDR is very compute intensive and will make an RPi run
too hot for passive cooling alone in some enclosures.
See [README-RPi](doc/README-RPi.md) for
more details about hardware, wiring, and configuration.

### VUmonitor application

`vumonitor` is an optional application that may be started by `rsked`.
It continuously monitors the audio output level delivered to the Linux
sound system. If this is dead quiet for an extended period, it signals
`rsked`; it signals again when audio output resumes.  This allows `rsked`
to detect when a source has failed "silently". This condition can occur
with internet streams and broadcast radio stations.

### check_inet.sh application

`check_inet.sh` is an optional application that periodically verifies
that an internet connection usable for streaming is available.
`rsked` uses this datum to determine whether it is feasible to use an
internet streaming source.


## Compilation and Installation

For installation instructions see: [INSTALLATION](doc/INSTALLATION.md)

We hope to provide occasional binary releases; check the releases link.
Compilation from source should be straightforward.

For a reference to the configuration files and command line
options for `rsked` and `cooling`, 
see: [CONFIGURATION](doc/CONFIGURATION.md)

[README-RPi](doc/README-RPi.md) covers instructions particular
to the Raspberry Pi platform.

## Known problems and limitations

Limitation: `rsked` only runs under Linux.

`rsked` is experimental software.

## License

`rsked` sources are covered by the Apache 2.0 License. 
See: [LICENSE](LICENSE.txt)  External components referenced
by `rsked` are covered by their own licenses.
