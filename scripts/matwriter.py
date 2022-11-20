#!/usr/bin/python

# Class used to control the Adafruit USB Backback LCD display via
# matrix orbital commands.  This might work with other compatible
# USB/serial displays.

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

import serial
import time
from datetime import datetime

# DEFAULTS
ROWS = 2
COLS = 16
BAUD = 9600

class MatWriter:
    """Control the Adafruit USB/Serial Backpack LCD display via Matrix
    Orbital commands. Designed to scroll a marquee title on line 1 and
    display the date-time on line 2. Timing of updates is not handled
    by this class.
    """

    Colors = { 'blue': (0x60, 0x60, 0xFF),
               'amber':(0xFF, 0x20, 0x00),
               'green':(0x40, 0xFF, 0x40), 
               'white':(0xFF, 0xFF, 0xFF) };

    def __init__(self,devnode,*,rows=ROWS,cols=COLS,baud=BAUD):
        self._devnode = devnode
        self._serial = serial.Serial(devnode, BAUD, timeout=1)
        self._rows = rows
        self._cols = cols
        self._baud = baud
        self._marquee = ''
        self._bastr = bytearray()
        self._hpos = 0
        self._time_str = ''
        self.set_size(rows,cols)
        self.clear_screen()

    def reset(self):
        self._marquee = ''
        self._bastr = bytearray()
        self._hpos = 0
        self._time_str = ''
        self.clear_screen()

    @property
    def marquee(self):
        return self._marquee

    @marquee.setter
    def marquee( self, lstr ):
        '''
        Set the marquee scroll string to lstr. Call hscroll periodically
        afterwards to make it scroll to the left.
        Example: mwriter.marquee = "Monkeys: The Porpoise Song"
        '''
        if self._marquee == lstr: # same string? no change
            return
        self._marquee = lstr
        nc = self._cols
        self._bastr = bytearray(lstr,encoding='utf8')
        # short strings require no scrolling at all
        if len(self._bastr) < nc:
            self.home()
            self._serial.write( self._bastr.ljust(nc) )
            return 0
        # we must scroll: append a blank for wraparound
        self._bastr.insert(0, ord(' '))
        self.home()
        self._serial.write( self._bastr[0:nc] )

    def hscroll(self):
        '''Horizontal scroll the marquee, if required. Returns
        the index of the leading character (0 for start of string).
        '''
        ns = len(self._bastr)
        if (ns < self._cols):
            return 0            # no scrolling needed--short string
        self._hpos = (self._hpos + 1) % ns
        self.home()
        self._serial.write( self.scroll_to(self._bastr, self._hpos) )
        return self._hpos

    def write_command( self, commandlist ):
        'Argument should be a list of integers'
        commandlist.insert(0, 0xFE)
        self._serial.write(bytearray(commandlist))

    def write_str( self, str ):
        self._serial.write(bytearray(str,encoding='utf8'));

    def clear_screen(self):
        self.write_command([0x58]) 

    def set_size(self,rows,cols):
        self.write_command([0xD1, cols, rows]);

    def backlight_on(self,timeout=0):
        'turn on display backlight; timeout minutes ignored'
        self.write_command([0x42, timeout]) 

    def backlight_off(self):
        'turn off display backlight'
        self.write_command([0x46]) 

    def backlight_rgb( self, r, g, b ):
        self.write_command([0xD0, r, g, b])

    def backlight_color( self, cname ):
        m = self.Colors.get(cname)
        if None == m:
            print("Error: No background color named '%s'" % cname)
        else:
            (r,g,b) = m
            self.backlight_rgb(r,g, b)

    def cursor_to( self, *, row, col ):
        # ToDO: validate row and col magnitude
        self.write_command([0x47, col, row])

    def home(self):
        self.write_command([0x48])

    def set_boot_message(self,str):
        'Set initial boot message to string str, up to rows*cols length.'
        cap = (self._rows * self._cols)  # screen capacity in chars
        if len(str) >= cap:
            bstr = [ord(c) for c in list(str[0:cap])]
        else:
            bstr = [ord(c) for c in list(str.ljust(cap))]
        bstr.insert(0, 0x40)
        self.write_command( bstr )

    def scroll_to( self, str, offset ):
        '''Returns a subset of str scrolled to offset, with wraparound
        modulo the column width window of the display.
        '''
        slen = len(str)
        winsize = self._cols  # window is columns in the line
        n = (winsize - 1)   # final window index
        if slen <= winsize :
            return str.ljust(winsize)
        # too long to fit in window: compute marquee portal
        moff = offset % slen
        if 0 == moff :
            return str[ 0:winsize ]
        else:
            k = moff + n
            if (k >= slen):
                b = winsize - (slen - moff)
                return str[moff:slen] + str[0:b]
            else:
                return str[moff:(moff+winsize)]

    def update_time(self):
        'Writes the date and time on line 2 of the display'
        s = datetime.now().strftime(" %m/%d/%y %H:%M")
        if s != self._time_str:
            self._time_str = s
            self.cursor_to( row=2, col=1 )
            self.write_str( self._time_str )

################################################################
