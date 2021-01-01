## Version 1.0.5, 2021-01-01

- Support for VLC Media player as a player option
- Support for player preference by media type in rsked.json
- Bluetooth monitoring and configuration utility, with documentation.

## Version 1.0.3,  2020-10-20

This release fixes bugs and introduces some significant enhancements
and changes.

- Schedule schema has changed to 2.0
- Formal JSON schema for validating 2.0 schedules
- Improvements to documentation
- Script to create deployable releases
- Script to verify a deployment configuration
- Support for flac format audio sources
- Experimental monitoring option using Bluetooth

The most noticeable change is in the `schedule.json` configuration
file.  The new schema is similar to the earlier one, but is simpler in
design.  Any existing schedules will need to be upgraded--no backward
compatibility.  Upgrading is a relatively simple manual process.  (In
this early stage project, hopefully this affects exactly nobody.)  It
sets the stage for a web-based graphical editor for schedules, which
is under development, to be released "soon".

The experimental Bluetooth service allows operators to check the
status and perform some administrative actions on a unit in
environments with restricted or nonexistent Wi-Fi or Ethernet. It is
intended to eventually support more full-featured configuration
capabilities.


## Version 0.6.5,  2020-08-02

This was effectively the first public release of `rsked`.
Some feedback suggested it was fairly simple to compile, but tricky
to configure; future releases will attempt to rectify this.

