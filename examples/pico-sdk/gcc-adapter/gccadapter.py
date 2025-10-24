# Quick and dirty client for the GameCube adapter HID interface
# Requires hidapi: pip install hidapi

import hid
import argparse

# USB Vendor ID and Product ID for the GameCube adapter
VID = 0x057E
PID = 0x0337

# HID report IDs
SET_MOTOR = 0x11
READ_ORIGIN = 0x12
START_POLLING = 0x13
STOP_POLLING = 0x14
RESET = 0x15


def open_dev():
    h = hid.device()
    h.open(VID, PID)
    h.set_nonblocking(True)
    return h


def send_hid_report(tx):
    h = open_dev()
    try:
        h.write(bytes(tx))
    finally:
        h.close()


def read_hid_reports(args):
    h = open_dev()
    try:
        while True:
            rx = h.read(64)
            if rx:
                print(" ".join(f"{b:02x}" for b in rx))
    except KeyboardInterrupt:
        pass
    finally:
        h.close()


def main():
    parser = argparse.ArgumentParser(description="GameCube adapter HID tool")
    sub = parser.add_subparsers(dest="command", required=True)

    # set_motor
    set_motor = sub.add_parser("set_motor", help="Send a 'set motor' HID report")
    set_motor.add_argument("channels", nargs=4, type=int, help="ch0 ch1 ch2 ch3")
    set_motor.set_defaults(
        func=lambda args: send_hid_report([SET_MOTOR] + args.channels)
    )

    # read_origin
    read_origin = sub.add_parser("read_origin", help="Send a 'read origin' HID report")
    read_origin.set_defaults(func=lambda args: send_hid_report([READ_ORIGIN]))

    # start_polling
    start_polling = sub.add_parser(
        "start_polling", help="Send a 'start polling' HID report"
    )
    start_polling.set_defaults(func=lambda args: send_hid_report([START_POLLING]))

    # stop_polling
    stop_polling = sub.add_parser(
        "stop_polling", help="Send a 'stop polling' HID report"
    )
    stop_polling.set_defaults(func=lambda args: send_hid_report([STOP_POLLING]))

    # reset
    reset = sub.add_parser("reset", help="Send a 'reset' HID report")
    reset.set_defaults(func=lambda args: send_hid_report([RESET]))

    # listen
    listen = sub.add_parser("listen", help="Continuously read and print HID reports")
    listen.set_defaults(func=read_hid_reports)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
