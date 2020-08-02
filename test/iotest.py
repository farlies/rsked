#!/usr/bin/python

import time
import RPi.GPIO as GPIO
GPIO.setmode(GPIO.BCM) # GPIO Numbers instead of pin numbers
GPIO.setwarnings(False)

print "Test the radio GPIO pins"

FAN_GPIO = 4
GRN_LED_GPIO = 17
BUTTON_GPIO  = 18
RED_LED_GPIO = 27

GPIO.setup(GRN_LED_GPIO, GPIO.OUT) # GPIO Assign mode
GPIO.setup(RED_LED_GPIO, GPIO.OUT) # GPIO Assign mode
GPIO.setup(FAN_GPIO, GPIO.OUT) # GPIO Assign mode
GPIO.setup(BUTTON_GPIO, GPIO.IN) # GPIO Assign mode

print "Fan ON"
GPIO.output(FAN_GPIO, GPIO.HIGH) # fan on

print "Red ON"
GPIO.output(RED_LED_GPIO, GPIO.HIGH) # red on
time.sleep(5)

if GPIO.input(BUTTON_GPIO):
    print "Button is High"
else:
    print "Button is Low"

print "Red OFF"
GPIO.output(RED_LED_GPIO, GPIO.LOW) # off
time.sleep(2)

print "Green ON"
GPIO.output(GRN_LED_GPIO, GPIO.HIGH) # on
time.sleep(5)

print "Green OFF"
GPIO.output(GRN_LED_GPIO, GPIO.LOW) # off

print "Fan OFF"
GPIO.output(FAN_GPIO, GPIO.LOW) # off

print "Hold down button..."
time.sleep(2)
if GPIO.input(BUTTON_GPIO):
    print "Button is High"
else:
    print "Button is Low"

GPIO.cleanup()
