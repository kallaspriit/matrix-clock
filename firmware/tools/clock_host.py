#!/usr/bin/env python3
"""Host side companion for the led-clock firmware.

Keeps the panel's clock synced over USB serial and forwards anything you type straight through to
the firmware, so the same window works for both driving the clock and poking at the test suite.

    pip install pyserial
    python tools/clock_host.py                 # auto-detect the Pico
    python tools/clock_host.py --port COM5
    python tools/clock_host.py --notify 3 "Build finished"

Hook your mail poller into send_notification() to push unread counts at the panel.
"""

import argparse
import calendar
import sys
import threading
import time

import serial
from serial.tools import list_ports

RASPBERRY_PI_VID = 0x2E8A
SYNC_INTERVAL_SECONDS = 300


def find_port():
    """Returns the first serial port that looks like a Pico."""

    for port in list_ports.comports():
        if port.vid == RASPBERRY_PI_VID:
            return port.device

    return None


def local_epoch():
    """Epoch seconds that render as local wall clock time when treated as UTC.

    The firmware has no timezone database, so all DST and offset handling stays here.
    """

    return calendar.timegm(time.localtime())


def send(connection, line):
    connection.write((line + "\n").encode("utf-8"))
    connection.flush()


def send_notification(connection, count, text):
    send(connection, "notify {} {}".format(count, text))


def pump_output(connection):
    """Prints everything the firmware sends back, on its own thread."""

    while True:
        try:
            line = connection.readline()
        except (serial.SerialException, OSError):
            print("[disconnected]")

            return

        if line:
            sys.stdout.write(line.decode("utf-8", errors="replace"))
            sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(description="Drive the led-clock panel over USB serial.")
    parser.add_argument("--port", help="serial port, auto-detected when omitted")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--notify", nargs=2, metavar=("COUNT", "TEXT"), help="send one notification and exit")
    arguments = parser.parse_args()

    port = arguments.port or find_port()

    if port is None:
        print("error: no Pico found, pass --port explicitly")

        return 1

    connection = serial.Serial(port, arguments.baud, timeout=1)

    # The Pico reboots when the USB CDC port is opened, give it a moment to come back
    time.sleep(2.0)
    connection.reset_input_buffer()

    if arguments.notify:
        send_notification(connection, arguments.notify[0], arguments.notify[1])
        time.sleep(0.3)

        return 0

    print("connected on {}, type 'help' for firmware commands, ctrl-c to quit".format(port))

    threading.Thread(target=pump_output, args=(connection,), daemon=True).start()

    send(connection, "time {}".format(local_epoch()))
    send(connection, "mode clock")

    last_sync = time.time()

    try:
        while True:
            # Re-sync periodically, the firmware only carries time forward with millis()
            if time.time() - last_sync > SYNC_INTERVAL_SECONDS:
                send(connection, "time {}".format(local_epoch()))

                last_sync = time.time()

            line = sys.stdin.readline()

            if not line:
                break

            send(connection, line.rstrip("\n"))
    except KeyboardInterrupt:
        pass
    finally:
        connection.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
