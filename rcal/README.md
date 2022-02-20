# Installing RCAL Web Control

Apologies: These instructions are **under construction** and incomplete.
We hope to be filling in the missing pieces soon.


# Install Web Content

Install (or link) right here in `rcal/` two 3rd party tools:

1. jquery.js   == jQuery v3.5.1
2. fullcalendar-5.2.0.zip ==  FullCalendar v5.2.0 


Running`sudo ./install.sh` script will install  the web pages etc.
Aside: It must be run as `root`. It currently assumes an empty web root
and might not work right if existing kit is present.

In the web root, make `index.html` link to `logview.html` (for now):

```
sudo su -
cd /var/www/html
ln -s logview.html index.html
exit
```

# Web Server Configuration

This should work with most web servers that can support PHP.
I have tested with lighttpd and Apache. Configuration file changes
required are, of course, server-specific.

## lighttpd

As its name suggests, lighttpd is a web server with a small footprint,
and is more than adequate for rcal.

### PHP

Review the (slightly dated) tutorial here:
- https://redmine.lighttpd.net/projects/lighttpd/wiki/TutorialLighttpdAndPHP

FastCGI is preferred way to run PHP, but is not enabled by
default. Fix this in by installing a couple of mod files in the config
directory:

```
cd /etc/lighttpd/
sudo cp conf-available/10-fastcgi.conf conf-enabled/
sudo cp conf-available/15-fastcgi-php.conf conf-enabled/
```

There are many options for configuration of the fastCGI module:
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

TODO


## Catalog

You must prepare `catalog.json`, a file that describes the local
recorded music, even if no such files are to be programmed.
Here is an example of one way to do this:

```
cd
#sudo apt install mediainfo  # (package needed by rskrape.pl)

~/bin/rskrape.pl > catalog.json
sudo cp catalog.json /var/www/html/
```


## Sudo Policy

Since web server runs as user `www-data` with homedir `/var/www` and
has `nologin` as its shell it cannot readily run rsked, even in test
mode. However we get around this with a well configured *sudoers*
policy, edited by running `visudo`.  Should look roughly like:

```
# /etc/sudoers
Runas_Alias RSKED = pi
Cmnd_Alias  TESTRSKED = /var/www/misc-bin/testrsked.sh ""

# On all machines, www-data can run TESTRSKED command as user
# RSKED *without* providing a password
www-data  ALL = (RSKED) NOPASSWD: TESTRSKED
```

Observe closely the empty pair of double quotes at the end of the `Cmnd_Alias`.
Without this policy, rcal will not be permitted to save a schedule.
