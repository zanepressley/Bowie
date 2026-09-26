import socket
import struct
import sys
import termios
import tty
import select
import time


# ============================================
# ESP32 CONNECTION
# ============================================

ESP32_IP = "10.27.223.132"
ESP32_PORT = 4210

SPEED = 0.5
SERVO_STEP = 0.05
SEND_RATE = 20


sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


# ============================================
# SEND COMMAND
# ============================================

def send_command(left, right, servo_pos):

    # Safety limits
    left = max(0.0, min(1.0, left))
    right = max(0.0, min(1.0, right))
    servo_pos = max(0.0, min(1.0, servo_pos))

    # ESP32 TeleopCommand:
    #
    # float left_drive
    # float right_drive
    # float servo_pos
    # uint32_t timestamp
    #
    # Total packet size = 16 bytes

    packet = struct.pack(
        "<fffI",
        left,
        right,
        servo_pos,
        0
    )

    sock.sendto(
        packet,
        (ESP32_IP, ESP32_PORT)
    )


# ============================================
# READ KEY
# ============================================

def get_key():

    if select.select([sys.stdin], [], [], 0)[0]:
        return sys.stdin.read(1)

    return None


# ============================================
# MAIN
# ============================================

old_settings = termios.tcgetattr(sys.stdin)

try:

    tty.setcbreak(sys.stdin)

    print()
    print("================================")
    print("       ESP32 TELEOP CONTROL")
    print("================================")
    print()
    print("DRIVE")
    print("W     = Forward")
    print("A     = Turn Left")
    print("D     = Turn Right")
    print("SPACE = Stop")
    print()
    print("SERVO")
    print("Q     = Servo Down")
    print("E     = Servo Up")
    print()
    print("X     = Quit")
    print()
    print("Sending to:", ESP32_IP)
    print("Port:", ESP32_PORT)
    print()

    # Motor commands
    left = 0.0
    right = 0.0

    # Servo starts at center
    servo_pos = 0.5

    # Used for automatic motor timeout
    last_drive_key_time = time.time()

    while True:

        key = get_key()

        if key is not None:

            key = key.lower()

            # ====================================
            # DRIVE COMMANDS
            # ====================================

            if key == "w":

                left = SPEED
                right = SPEED

                last_drive_key_time = time.time()

            elif key == "a":

                left = SPEED
                right = 0.0

                last_drive_key_time = time.time()

            elif key == "d":

                left = 0.0
                right = SPEED

                last_drive_key_time = time.time()

            elif key == " ":

                left = 0.0
                right = 0.0

                last_drive_key_time = time.time()

            # ====================================
            # SERVO COMMANDS
            # ====================================

            elif key == "q":

                servo_pos -= SERVO_STEP

                servo_pos = max(
                    0.0,
                    min(1.0, servo_pos)
                )

            elif key == "e":

                servo_pos += SERVO_STEP

                servo_pos = max(
                    0.0,
                    min(1.0, servo_pos)
                )

            # ====================================
            # QUIT
            # ====================================

            elif key == "x":

                break

        # ========================================
        # MOTOR SAFETY TIMEOUT
        # ========================================

        if time.time() - last_drive_key_time > 0.15:

            left = 0.0
            right = 0.0

        # ========================================
        # SEND COMMAND
        # ========================================

        send_command(
            left,
            right,
            servo_pos
        )

        # ========================================
        # STATUS DISPLAY
        # ========================================

        print(
            f"\rL: {left:.2f} | "
            f"R: {right:.2f} | "
            f"SERVO: {servo_pos:.2f}",
            end="",
            flush=True
        )

        time.sleep(1 / SEND_RATE)


finally:

    # ============================================
    # SAFETY STOP
    # ============================================

    send_command(
        0.0,
        0.0,
        servo_pos
    )

    termios.tcsetattr(
        sys.stdin,
        termios.TCSADRAIN,
        old_settings
    )

    sock.close()

    print()
    print("Stopped.")
