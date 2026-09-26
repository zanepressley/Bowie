import socket
import struct
import sys
import termios
import tty
import select
import time


ESP32_IP = "10.27.223.132"
ESP32_PORT = 4210

SPEED = 0.5
SEND_RATE = 20


sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


def send_command(left, right):

    # Never allow negative motor commands
    left = max(0.0, min(1.0, left))
    right = max(0.0, min(1.0, right))

    packet = struct.pack(
        "<ffI",
        left,
        right,
        0
    )

    sock.sendto(
        packet,
        (ESP32_IP, ESP32_PORT)
    )


def get_key():

    if select.select([sys.stdin], [], [], 0)[0]:
        return sys.stdin.read(1)

    return None


old_settings = termios.tcgetattr(sys.stdin)

try:

    tty.setcbreak(sys.stdin)

    print("================================")
    print("       ESP32 TELEOP CONTROL")
    print("================================")
    print()
    print("W     = Forward")
    print("A     = Turn Left")
    print("D     = Turn Right")
    print("SPACE = Stop")
    print("Q     = Quit")
    print()
    print("Sending to:", ESP32_IP)
    print("Port:", ESP32_PORT)
    print()

    left = 0.0
    right = 0.0

    last_key_time = time.time()

    while True:

        key = get_key()

        if key is not None:

            key = key.lower()
            last_key_time = time.time()

            if key == "w":
                left = SPEED
                right = SPEED

            elif key == "a":
                left = SPEED
                right = 0.0

            elif key == "d":
                left = 0.0
                right = SPEED

            elif key == " ":
                left = 0.0
                right = 0.0

            elif key == "q":
                break

        # Stop if no key has been pressed recently
        if time.time() - last_key_time > 0.15:
            left = 0.0
            right = 0.0

        send_command(left, right)

        print(
            f"\rL: {left:.2f} | R: {right:.2f}",
            end="",
            flush=True
        )

        time.sleep(1 / SEND_RATE)


finally:

    # Always stop motors when exiting
    send_command(0.0, 0.0)

    termios.tcsetattr(
        sys.stdin,
        termios.TCSADRAIN,
        old_settings
    )

    sock.close()

    print("\nStopped.")