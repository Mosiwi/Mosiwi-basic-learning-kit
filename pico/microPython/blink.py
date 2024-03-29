# https://docs.micropython.org/en/latest/rp2/quickref.html
import time                 # Importing the time class
from machine import Pin     # Import the Pin class from the machine module.

p25 = Pin(25, Pin.OUT)      # create output pin on GPIO25

while(1):
    p25.on()                # set pin to "on" (high) level
    time.sleep(1)           # sleep for 1 second
    p25.off()               # set pin to "off" (low) level
    time.sleep(1)           # sleep for 1 second

