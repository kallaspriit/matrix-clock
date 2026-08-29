#!/usr/bin/env python3
"""Host side companion for the matrix-clock firmware.

Keeps the panel's clock synced over USB serial. Interactively it also forwards anything you type
straight through to the firmware, so one window drives both the clock and the test suite.

    pip install pyserial
    pip install pystray pillow                          # only needed for --tray

    python tools/clock_host.py                          # interactive, auto-detect the Pico
    python tools/clock_host.py --port COM5
    python tools/clock_host.py --notify 3 "Build finished"
    python tools/clock_host.py --tray --log host.log    # tray icon, used by the scheduled task
    python tools/clock_host.py --daemon --log host.log  # headless, no UI at all

Daemon and tray modes read no stdin and reconnect on their own, which is what makes them survivable
as a Windows startup task: the console is gone under pythonw.exe, and the Pico's COM port often has
not enumerated yet at logon.

Hook your mail poller into ClockHost.notify() to push unread counts at the panel.
"""

import argparse
import calendar
import os
import sys
import threading
import time

import serial
from serial.tools import list_ports

RASPBERRY_PI_VID = 0x2E8A
SYNC_INTERVAL_SECONDS = 300
HEARTBEAT_SECONDS = 30
RECONNECT_SECONDS = 5
RELEASE_SECONDS = 60
LOG_LIMIT_BYTES = 1024 * 1024

log_file = None
log_path = None


def log(message):
    """Writes to the log file when one is configured, otherwise to stdout."""

    stamped = "{} {}".format(time.strftime("%Y-%m-%d %H:%M:%S"), message)

    if log_file is None:
        print(stamped)
        sys.stdout.flush()

        return

    log_file.write(stamped + "\n")
    log_file.flush()


def open_log(path):
    global log_file, log_path

    directory = os.path.dirname(os.path.abspath(path))

    if directory:
        os.makedirs(directory, exist_ok=True)

    # Truncate rather than rotate, this only ever holds connection events
    if os.path.exists(path) and os.path.getsize(path) > LOG_LIMIT_BYTES:
        os.remove(path)

    log_file = open(path, "a", encoding="utf-8")
    log_path = os.path.abspath(path)


def find_port():
    """Returns the first serial port that looks like a Pico."""

    for port in list_ports.comports():
        if port.vid == RASPBERRY_PI_VID:
            return port.device

    return None


def local_epoch():
    """Epoch seconds that render as local wall clock time when treated as UTC.

    The firmware has no timezone database, so all DST and offset handling stays here. Because this
    is re-sent every few minutes, a DST transition corrects itself without anyone doing anything.
    """

    return calendar.timegm(time.localtime())


class ClockHost:
    """Owns the serial link and keeps it alive. Safe to drive from a UI thread.

    Coordination with the UI is deliberately just a few flags rather than a queue: every field here
    is written by one thread and read by the other, and none of the transitions need to be atomic.
    """

    def __init__(self, port, baud):
        self.requested_port = port
        self.baud = baud

        self.connection = None
        self.connected_port = None
        self.release_until = 0.0
        self.sync_requested = False
        self.stopping = False

    # --- things the tray menu calls ---

    def is_connected(self):
        return self.connection is not None

    def is_released(self):
        return time.monotonic() < self.release_until

    def status_text(self):
        if self.is_released():
            return "Port released for {}s".format(int(self.release_until - time.monotonic()))

        if self.is_connected():
            return "Connected on {}".format(self.connected_port)

        return "Waiting for the clock"

    def release(self, seconds=RELEASE_SECONDS):
        """Drops the port so a firmware upload can have it, then reconnects by itself."""

        self.release_until = time.monotonic() + seconds

        log("releasing the port for {}s".format(seconds))

    def reconnect(self):
        self.release_until = 0.0

        self._close()

    def request_sync(self):
        self.sync_requested = True

    def notify(self, count, text):
        if self.connection is None:
            return False

        try:
            self._send("notify {} {}".format(count, text))
        except (serial.SerialException, OSError):
            return False

        return True

    def stop(self):
        self.stopping = True

        self._close()

    # --- the worker ---

    def run_forever(self):
        log("host starting")

        while not self.stopping:
            if self.is_released():
                time.sleep(1.0)

                continue

            port = self.requested_port or find_port()

            if port is None:
                time.sleep(RECONNECT_SECONDS)

                continue

            try:
                self._open(port)
            except (serial.SerialException, OSError) as error:
                log("could not open {}: {}".format(port, error))
                time.sleep(RECONNECT_SECONDS)

                continue

            log("connected on {}".format(port))

            try:
                self._session()
            except (serial.SerialException, OSError) as error:
                log("link lost: {}".format(error))
            finally:
                self._close()

            if not self.stopping and not self.is_released():
                time.sleep(RECONNECT_SECONDS)

        log("host stopped")

    def _open(self, port):
        connection = serial.Serial(port, self.baud, timeout=1)

        # The board can reset when the CDC port is opened, give it a moment before talking to it
        time.sleep(2.0)
        connection.reset_input_buffer()

        self.connection = connection
        self.connected_port = port

    def _close(self):
        connection = self.connection

        self.connection = None
        self.connected_port = None

        if connection is not None:
            try:
                connection.close()
            except Exception:
                pass

    def _send(self, line):
        self.connection.write((line + "\n").encode("utf-8"))
        self.connection.flush()

    def _session(self):
        self._send("time {}".format(local_epoch()))
        self._send("mode clock")

        log("synced and switched to clock mode")

        last_sync = time.monotonic()
        last_beat = time.monotonic()

        while not self.stopping and not self.is_released():
            # Drain whatever the firmware replied, otherwise it backs up in the OS buffer
            waiting = self.connection.in_waiting

            if waiting:
                self.connection.read(waiting)

            now = time.monotonic()

            if self.sync_requested or now - last_sync >= SYNC_INTERVAL_SECONDS:
                self.sync_requested = False

                self._send("time {}".format(local_epoch()))

                last_sync = now
                last_beat = now
            elif now - last_beat >= HEARTBEAT_SECONDS:
                # Cheap write purely so an unplugged board is noticed in seconds rather than minutes
                self._send("ping")

                last_beat = now

            time.sleep(1.0)


def make_icon_image(connected):
    """A small gradient block in the panel's own palette, greyed out when disconnected."""

    from PIL import Image, ImageDraw

    size = 64
    top, bottom = ((255, 140, 0), (0, 255, 140)) if connected else ((110, 110, 110), (60, 60, 60))

    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    for y in range(size):
        fraction = y / float(size - 1)
        colour = tuple(int(top[i] + (bottom[i] - top[i]) * fraction) for i in range(3))

        draw.line([(0, y), (size, y)], fill=colour + (255,))

    # Knock two slots out so it reads as a display rather than a coloured blob
    draw.rectangle([10, 26, 26, 38], fill=(0, 0, 0, 0))
    draw.rectangle([38, 26, 54, 38], fill=(0, 0, 0, 0))

    return image


def run_tray(host):
    try:
        import pystray
    except ImportError:
        log("pystray or pillow not installed, falling back to headless. pip install pystray pillow")
        host.run_forever()

        return

    def on_release(icon, item):
        host.release()

    def on_reconnect(icon, item):
        host.reconnect()

    def on_sync(icon, item):
        host.request_sync()

    def on_open_log(icon, item):
        if log_path and os.path.exists(log_path):
            os.startfile(log_path)

    def on_quit(icon, item):
        host.stop()
        icon.stop()

    menu = pystray.Menu(
        pystray.MenuItem(lambda item: host.status_text(), lambda icon, item: None, enabled=False),
        pystray.Menu.SEPARATOR,
        pystray.MenuItem("Sync time now", on_sync),
        pystray.MenuItem("Release port for {}s (to flash firmware)".format(RELEASE_SECONDS), on_release),
        pystray.MenuItem("Reconnect now", on_reconnect),
        pystray.Menu.SEPARATOR,
        pystray.MenuItem("Open log", on_open_log, enabled=lambda item: log_path is not None),
        pystray.MenuItem("Quit", on_quit),
    )

    icon = pystray.Icon("matrix-clock", make_icon_image(False), "Matrix clock", menu)

    def refresh():
        """Repaints the icon only when the state actually changes, to avoid tray flicker."""

        shown = None

        while not host.stopping:
            connected = host.is_connected() and not host.is_released()

            if connected != shown:
                icon.icon = make_icon_image(connected)
                shown = connected

            icon.update_menu()

            time.sleep(2.0)

    threading.Thread(target=host.run_forever, daemon=True).start()
    threading.Thread(target=refresh, daemon=True).start()

    icon.run()


def pump_output(connection):
    """Prints everything the firmware sends back, on its own thread. Interactive mode only."""

    while True:
        try:
            line = connection.readline()
        except (serial.SerialException, OSError):
            print("[disconnected]")

            return

        if line:
            sys.stdout.write(line.decode("utf-8", errors="replace"))
            sys.stdout.flush()


def run_interactive(port, baud):
    connection = serial.Serial(port, baud, timeout=1)

    time.sleep(2.0)
    connection.reset_input_buffer()

    print("connected on {}, type 'help' for firmware commands, ctrl-c to quit".format(port))

    threading.Thread(target=pump_output, args=(connection,), daemon=True).start()

    def send(line):
        connection.write((line + "\n").encode("utf-8"))
        connection.flush()

    send("time {}".format(local_epoch()))
    send("mode clock")

    last_sync = time.monotonic()

    try:
        while True:
            if time.monotonic() - last_sync > SYNC_INTERVAL_SECONDS:
                send("time {}".format(local_epoch()))

                last_sync = time.monotonic()

            line = sys.stdin.readline()

            if not line:
                return

            send(line.rstrip("\n"))
    except KeyboardInterrupt:
        pass
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser(description="Drive the matrix-clock panel over USB serial.")
    parser.add_argument("--port", help="serial port, auto-detected when omitted")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--notify", nargs=2, metavar=("COUNT", "TEXT"), help="send one notification and exit")
    parser.add_argument("--tray", action="store_true", help="run with a system tray icon")
    parser.add_argument("--daemon", action="store_true", help="run headless, no stdin, reconnect forever")
    parser.add_argument("--log", help="append connection events to this file instead of stdout")
    arguments = parser.parse_args()

    if arguments.log:
        open_log(arguments.log)

    if arguments.tray or arguments.daemon:
        host = ClockHost(arguments.port, arguments.baud)

        if arguments.tray:
            run_tray(host)
        else:
            host.run_forever()

        return 0

    port = arguments.port or find_port()

    if port is None:
        print("error: no Pico found, pass --port explicitly")

        return 1

    if arguments.notify:
        connection = serial.Serial(port, arguments.baud, timeout=1)

        time.sleep(2.0)
        connection.write("notify {} {}\n".format(arguments.notify[0], arguments.notify[1]).encode("utf-8"))
        connection.flush()
        time.sleep(0.3)
        connection.close()

        return 0

    run_interactive(port, arguments.baud)

    return 0


if __name__ == "__main__":
    sys.exit(main())
