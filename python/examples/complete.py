#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Python example program.

Complete program to demonstrate the usage
of the swig generated python wrapper

You need to build and install the wrapper first"""

import os
import sys
import briteblox1 as briteblox
import time

# version
print ('version: %s\n' % briteblox.__version__)

# initialize
britebloxc = briteblox.new()
if britebloxc == 0:
    print('new failed: %d' % ret)
    os._exit(1)

# try to list briteblox devices 0x7AD0
ret, devlist = briteblox.usb_find_all(britebloxc, 0x0403, 0x7AD0)
if ret <= 0:
    ret, devlist = briteblox.usb_find_all(britebloxc, 0x0403, 0x7AD0)

if ret < 0:
    print('briteblox_usb_find_all failed: %d (%s)' %
          (ret, briteblox.get_error_string(britebloxc)))
    os._exit(1)
print('devices: %d' % ret)
curnode = devlist
i = 0
while(curnode != None):
    ret, manufacturer, description, serial = briteblox.usb_get_strings(
        britebloxc, curnode.dev)
    if ret < 0:
        print('briteblox_usb_get_strings failed: %d (%s)' %
              (ret, briteblox.get_error_string(britebloxc)))
        os._exit(1)
    print('#%d: manufacturer="%s" description="%s" serial="%s"\n' %
          (i, manufacturer, description, serial))
    curnode = curnode.next
    i += 1

# open usb
ret = briteblox.usb_open(britebloxc, 0x0403, 0x7AD0)
if ret < 0:
    print('unable to open briteblox device: %d (%s)' %
          (ret, briteblox.get_error_string(britebloxc)))
    os._exit(1)


# bitbang
ret = briteblox.set_bitmode(britebloxc, 0xff, briteblox.BITMODE_BITBANG)
if ret < 0:
    print('Cannot enable bitbang')
    os._exit(1)
print('turning everything on')
briteblox.write_data(britebloxc, chr(0xff), 1)
time.sleep(1)
print('turning everything off\n')
briteblox.write_data(britebloxc, chr(0x00), 1)
time.sleep(1)
for i in range(8):
    val = 2 ** i
    print('enabling bit #%d (0x%02x)' % (i, val))
    briteblox.write_data(britebloxc, chr(val), 1)
    time.sleep(1)
briteblox.disable_bitbang(britebloxc)
print('')


# read pins
ret, pins = briteblox.read_pins(britebloxc)
if (ret == 0):
    if sys.version_info[0] < 3:  # python 2
        pins = ord(pins)
    else:
        pins = pins[0]
    print('pins: 0x%x' % pins)


# read chip id
ret, chipid = briteblox.read_chipid(britebloxc)
if (ret == 0):
    print('chip id: %x\n' % chipid)


# read eeprom
eeprom_addr = 1
ret, eeprom_val = briteblox.read_eeprom_location(britebloxc, eeprom_addr)
if (ret == 0):
    print('eeprom @ %d: 0x%04x\n' % (eeprom_addr, eeprom_val))

print('eeprom:')
ret = briteblox.read_eeprom(britebloxc)
size = 128
ret, eeprom = briteblox.get_eeprom_buf(britebloxc, size)
if (ret == 0):
    for i in range(size):
        octet = eeprom[i]
        if sys.version_info[0] < 3:  # python 2
            octet = ord(octet)
        sys.stdout.write('%02x ' % octet)
        if (i % 8 == 7):
            print('')
print('')

# close usb
ret = briteblox.usb_close(britebloxc)
if ret < 0:
    print('unable to close briteblox device: %d (%s)' %
          (ret, briteblox.get_error_string(britebloxc)))
    os._exit(1)

print ('device closed')
briteblox.free(britebloxc)
