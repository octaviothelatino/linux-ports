#!/usr/bin/env python3

import ctypes
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: x11-click.py X Y", file=sys.stderr)
        return 2

    x = int(sys.argv[1])
    y = int(sys.argv[2])
    x11 = ctypes.CDLL("libX11.so.6")
    xtst = ctypes.CDLL("libXtst.so.6")

    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    xtst.XTestFakeMotionEvent.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_ulong,
    ]
    xtst.XTestFakeButtonEvent.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint,
        ctypes.c_int,
        ctypes.c_ulong,
    ]

    display = x11.XOpenDisplay(None)
    if not display:
        print("could not open DISPLAY", file=sys.stderr)
        return 1

    screen = 0
    xtst.XTestFakeMotionEvent(display, screen, x, y, 0)
    xtst.XTestFakeButtonEvent(display, 1, 1, 0)
    xtst.XTestFakeButtonEvent(display, 1, 0, 0)
    x11.XFlush(display)
    x11.XCloseDisplay(display)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
