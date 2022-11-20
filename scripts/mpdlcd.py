#!/usr/bin/python3
# This program will watch mpd and make the matrix LCD display
# reflect the player state
#   play : green background, song title
#  pause : amber background, song title
#   stop : backlight off, date/time

## vvv change the logging directory below to site logdir vvv

# Part of the rsked package.
# Copyright 2022 Steven A. Harp
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#########################################################################


import time
import sys
import logging
from matwriter import MatWriter

### ***  Change this to a valid log directory ***
LOG_FILENAME="/home/qrhacker/logs/mpdlcd.log"

Logger = None

USOCK_NAME = "/var/run/mpd/socket"

# Timing
SCROLL_INTERVAL = 0.333
POLL_CYCLE = 30
LINGER_CYCLE = 30

Poller = None
lcdDisplay = None

#................................................................


#-------------------------------------------------------------------
# loosely based on the error handling example in github python-mpd2
# https://github.com/Mic92/python-mpd2

from mpd import MPDClient, MPDError, CommandError


class PollerError(Exception):
    """Fatal error in poller."""


class MpdPoller(object):
    def __init__(self,host="localhost",port="6600",password=None):
        self._host = host
        self._port = port
        self._password = password
        self._state = 'unknown'
        self._run = True
        self._signum = 0
        self._song = ''
        self._nerrs = 0
        self._client = MPDClient()

    def runp(self):
        return self._run

    @property
    def nerrs(self):
        'Number of contiguous errors; resets after successful poll'
        return self._nerrs

    def sigexit(self):
        return self._signum

    def stop(self,signum):
        self._run = False
        self._signum = signum

    def connect(self):
        """Try to connect to mpd"""
        try:
            self._client.connect( self._host, self._port )
            print("Connection to MPD established", file=sys.stderr);
            Logger.info("Connection to MPD established")
        except ConnectionRefusedError as err:
            self._nerrs += 1
            raise PollerError("Connection to MPD Refused")
        except IOError as err:
            errno, strerror = err
            self._nerrs += 1
            raise PollerError("Could not connect to '%s': %s" % (self._host, strerror))
        except MPDError as e:
            self._nerrs += 1
            raise PollerError("Could not connect to '%s': %s" % (self._host, e))

    def disconnect(self):
        try:
            self._client.close()
        except (MPDError,IOError):
            pass

        try:
            self._client.disconnect()
        except (MPDError,IOError):
            # bad client...replacing
            self._client = MPDClient()


    def poll(self):
        'returns tuple (newstate,newsong); elt==False if no change'
        global Logger
        song = None
        newsong = False
        newstate = False
        try:
            status = self._client.status()
            song = self._client.currentsong()
            self._nerrs = 0
        except (MPDError, IOError):
            self.disconnect()
            try:
                self.connect()
            except PollerError as e:
                self._nerrs += 1
                raise PollerError("Reconnecting failed: %s" %e)
            #
            try:
                status = self._client.status()    # a dictionary
                song = self._client.currentsong() # a dictionary
            except (MPDError,IOError) as e:
                self._nerrs += 1
                raise PollerError("Could not get status/song: %s" % e)
        #-------
        st = status['state']
        if st == 'play' or st == 'pause':
            if 'artist' in song:
                at_song = "%s: %s" % (song['artist'],song.get('title','?'))
            else:
                # artist is commonly missing in network streams
                at_song = song.get('title',"?")
        else:
            at_song = ''
        if (self._song  != at_song):
            self._song = at_song
            newsong = at_song
            #print("new song='%s'" % self._song, file=sys.stderr)
        if (self._state != st):
            self._state = st
            newstate = st
            #print("Poller: new mpd status='%s'" % st, file=sys.stderr)
            Logger.info("Poller: new mpd status='%s'" % st)
        return (newstate,newsong)

#-------------------------------------------------------------------

def display_state( display, newstate ):
    'Adjust the backlight to reflect play state'
    if newstate == 'play':
        display.backlight_color('green')
        display.backlight_on()
    elif newstate == 'pause':
        display.backlight_color('amber')
        display.backlight_on()
    else:
        display.backlight_off()

def display_song( display, newsong ):
    if newsong == '':
        display.marquee = '     Stopped'
    else:
        display.marquee = newsong

def try_pollmpd( poller, display ):
    'ask mpd for state and song, updating display as needed'
    try:
        (newstate,newsong) = poller.poll()
        if newstate != False:
            display_state( display, newstate )
        if newsong != False:
            display_song( display, newsong )
    except PollerError as e:
        if (poller.nerrs > 20):
            raise PollerError("Too many errors--abort on %s" % e)
        print("Poller error: %s will retry" % e,file=sys.stderr)
        Logger.warning("Poller error: %s will retry" % e)

# MAIN montioring loop:

def main():
    from time import sleep
    global Poller
    #global lcdDisplay
    #global Logger
    Poller = MpdPoller(USOCK_NAME)
    sleep(6); # give mpd time to boot up
    cycle = (POLL_CYCLE - 1)   # 1st iteration will poll
    linger = 0
    #-------------------------------------
    Poller.connect()
    while Poller.runp():
        sleep(SCROLL_INTERVAL)
        cycle += 1
        if cycle == POLL_CYCLE:
            try_pollmpd( Poller, lcdDisplay )
            cycle = 0
            lcdDisplay.update_time()
        else:
            # we will linger a few seconds at start of title
            if linger==0:
                if 0 == lcdDisplay.hscroll():
                    linger = LINGER_CYCLE
            else:
                linger -= 1
    #-------------------------------------
    print("Poller exits on signal %d" % Poller.sigexit(), file=sys.stderr)
    Logger.error("Poller exits on signal %d" % Poller.sigexit())
    Poller.disconnect()


#-------------------------------------------------------------------

# when Ctrl-c et al is pressed tell the poller to exit

def ihandler(signum,frame):
    global Poller
    Poller.stop(signum)

if __name__ == "__main__":
    import sys
    import signal

    # First establish logging!
    Logger = logging.getLogger()
    logging.basicConfig(filename=LOG_FILENAME,format='%(asctime)s %(message)s',
                        filemode='w')
    Logger.setLevel(logging.INFO)
    Logger.info("mpdlcd starts...")

    PORT = "/dev/ttyACM0"
    # get serial port from command line or default to the usual
    if len(sys.argv) != 2:
        port=PORT
    else:
        port=sys.argv[1]
        print("Serialport=%s" % port)

    lcdDisplay = MatWriter(port)

    try:
        signal.signal(signal.SIGINT, ihandler)
        main()

    except PollerError as e:
        print("Fatal poller error: %s" % e,file=sys.stderr)
        Logger.error("Fatal poller error: %s" % e)
        sys.exit(1)

    except Exception as e:
        print("Unexpected exception: %s" % e, file=sys.stderr)
        Logger.error("Unexpected exception: %s" % e)
        sys.exit(2)

    except:
        print("Abnormal exit of mpdlcd (3)", file=sys.stderr)
        Logger.error("Abnormal exit of mpdlcd (3)")
        sys.exit(3)
    #----assume somehow we chose to terminate---
    lcdDisplay.clear_screen()
    lcdDisplay.backlight_off()
    Logger.info("Normal exit of mpdlcd")
    print("Normal exit of mpdlcd", file=sys.stderr)
    sys.exit(0)



