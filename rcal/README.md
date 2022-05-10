# Installing RCAL Web Control

**rcal** is a template for a web site that can be served directly from
the rsked device.  It offers the ability to view and edit the
schedule, to view rsked logs, and pause or resume the player. This
facility is entirely optional, and it should only be enabled in a
manner that is consistent with local network security. It is designed
to be self contained, and does not require internet access.  (We do
*not* recommend exposing this directly to the internet unless you
are familiar with the security measures required.)

# Install Web Content

Install (or link) in the `rcal/` subdirectory two 3rd party tools:

1. jquery.js: jQuery v3.5.1 or later https://jquery.com/
2. fullcalendar-5.2.0.zip: v5.2.0 or later https://fullcalendar.io/


Run`sudo ./install.sh` to install the web pages and supporting kit to
the web root (by default `/var/www`).  **First**, look over the script
and adjust any of the variables to reflect your local system's web
server and the correct pathnames for the 3rd party tools.  The script
must effectively be run as `root`. It currently assumes an empty web
root, and might not work right if existing kit is present.


# Web Server Configuration

The `rcal` code should work with most web servers that can support PHP,
and most graphical browsers that support modern Javascript.
It has been tested with `lighttpd` (notes below) and `Apache` servers.

## lighttpd

As its name suggests, *lighttpd* is a web server with a small footprint,
and is more than adequate for rcal.

### lighttpd.conf

Directory `/etc/lighttpd` contains files and subdirectories that are
used to configure this server. The master configuration file is
`lighttpd.conf`.  Make sure this file includes the following to enable
php and to make `logview.html` a valid default web page:

```
index-file.names = ( "logview.html", "index.php", "index.html" )
static-file.exclude-extensions = ( ".php", ".pl", ".fcgi" )
```

Enable the following subsidiary configuration modules:
- 05-auth.conf  (optional, if you want to protect with authentication)
- 10-accesslog.conf (optional)
- 10-fastcgi.conf
- 10-ssl.conf (optional, if you want to use https)
- 15-fastcgi-php.conf

See the lighttpd documentation for details about setting options in these
files, e.g. creating user and group credentals, and notes below.

### FastCGI and PHP

Tutorial here:
- https://redmine.lighttpd.net/projects/lighttpd/wiki/TutorialLighttpdAndPHP

FastCGI is preferred way to run PHP, but is not enabled by
default. Copy files (as root or via sudo) from subdirectory
conf-available to conf-enabled and adjust as needed:


- https://redmine.lighttpd.net/projects/lighttpd/wiki/Docs_ModFastCGI

The following is a `15-fastcgi-php.conf` suitable for a very light duty
server.  The sole php process will be retired after 20 seconds of idle
time.

```
## minimal 15-fastcgi-php
## Start an FastCGI server for php (needs the php5-cgi package)
fastcgi.server += ( ".php" => 
	((
		"bin-path" => "/usr/bin/php-cgi",
		"socket" => "/var/run/lighttpd/php.socket",
		"max-procs" => 1,
		"min-procs" => 0,
		"idle-timeout" => 20,
		"bin-environment" => ( 
			"PHP_FCGI_CHILDREN" => "4",
			"PHP_FCGI_MAX_REQUESTS" => "10000"
		),
		"bin-copy-environment" => (
			"PATH", "SHELL", "USER"
		),
		"broken-scriptfilename" => "enable"
	))
)
```

### Authentication

*Digest* auth is an okay compromise between security and ease of
configuration. Don't use it to protect something really sensitive.
See:
https://redmine.lighttpd.net/projects/lighttpd/wiki/Docs_ModAuth#HTTP-Auth-methods

E.g. we set realm "room200" with user "rcal", and some secret password.
You will need to blind type the password below when running `htdigest`
(from the Apache suite):

```
htdigest -c  lighttpd.user 'room200' rcal
sudo cp lighttpd.user /etc/lighttpd/
```

Also, this bit of configuration should go in `/etc/lighttpd/conf-enabled/05-auth.conf`:

```
# digest auth for rcal website
server.modules                += ( "mod_auth", "mod_authn_file" )

auth.backend                 = "htdigest"
auth.backend.htdigest.userfile  = "/etc/lighttpd/lighttpd.user"
#auth.backend.htdigest.groupfile = "lighttpd.group"

# protecting entire site here ("")
auth.require  = ( 
      "" =>  ( 
				  "method"  => "digest",
				  "realm"   => "room200",
				  "require" => "valid-user"
				)
         )
```

(The web server will probably need to be restarted after configuring.)


## Apache

**TODO** The target functionality is the same as with lighttpd but
the configuration details are all different...


## Catalog.json

You must prepare `catalog.json`, a file that describes the local
recorded music, even if no such files are to be programmed.
This file should be installed in the html root of the server.
The included program `rskrape.pl` is an easy way to do this.
It uses the mediainfo utility to retrieve audio file information.
Example:

```
cd
#sudo apt install mediainfo  # (package needed by rskrape.pl)

~/bin/rskrape.pl > catalog.json
sudo cp catalog.json /var/www/html/
```


## Sudo Policy

`rcal` may be used without any control capabilities in a "read-only" mode.
Since web server runs as user `www-data` with home directory `/var/www` and
has `nologin` as its shell, it cannot readily control `rsked`, even in test
mode. `rcal` may  get around this with a well configured *sudoers*
policy, edited by running `visudo`.  It should include something like
he following lines:

```
# /etc/sudoers
Runas_Alias RSKED = pi
Cmnd_Alias  TESTRSKED = /var/www/misc-bin/testrsked.sh ""
Cmnd_Alias PAUSERSKED = /usr/bin/pkill -x -USR1 rsked

# On all machines, www-data can run TESTRSKED command as user
# RSKED *without* providing a password
www-data  ALL = (RSKED) NOPASSWD: TESTRSKED
www-data  ALL = (RSKED) NOPASSWD: PAUSERSKED
```

Modify these settings to match your local installation (user, commands).
Note well the empty pair of double quotes at the end of the `Cmnd_Alias`
for `TESTRSKED`.

