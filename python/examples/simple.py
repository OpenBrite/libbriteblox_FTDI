#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Python example program.

Small program to demonstrate the usage
of the swig generated python wrapper

You need to build and install the wrapper first"""

import briteblox1 as briteblox


def main():
    """Main program"""
    context = briteblox.new()

    version_info = briteblox.get_library_version()
    print("[BRITEBLOX version] major: %d, minor: %d, micro: %d"
          ", version_str: %s, snapshot_str: %s" %
         (version_info.major, version_info.minor, version_info.micro,
          version_info.version_str, version_info.snapshot_str))

    # try to open an briteblox 0x7AD0
    ret = briteblox.usb_open(context, 0x0403, 0x7AD0)
    if ret < 0:
        ret = briteblox.usb_open(context, 0x0403, 0x7AD0)

    print("briteblox.usb_open(): %d" % ret)
    print("briteblox.set_baudrate(): %d" % briteblox.set_baudrate(context, 9600))

    briteblox.free(context)

main()
