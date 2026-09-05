import socket
import json
import pyautogui

# Disable PyAutoGUI fail-safe and remove default delay between actions
pyautogui.FAILSAFE = False
pyautogui.PAUSE = 0.0

UDP_IP = "0.0.0.0"
UDP_PORT = 5000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"[*] Air Mouse UDP Server is listening on port {UDP_PORT}...")

# Residual values are used to preserve sub-pixel movement between packets
residual_x = 0.0
residual_y = 0.0

# Scroll multiplier: 120 is commonly treated as one standard mouse wheel step
SCROLL_MULTIPLIER = 120

while True:
    try:
        data, addr = sock.recvfrom(1024)
        message = data.decode('utf-8').strip()

        command = json.loads(message)

        # Add the new movement delta to the remaining fractional movement
        raw_dx = command.get("DeltaX", 0) + residual_x
        raw_dy = command.get("DeltaY", 0) + residual_y

        click = command.get("Click", 0)
        scroll = command.get("Scroll", 0)

        # Convert movement values to integers because PyAutoGUI requires pixel values
        move_x = int(raw_dx)
        move_y = int(raw_dy)

        # Store the fractional part for the next packet to avoid losing small movements
        residual_x = raw_dx - move_x
        residual_y = raw_dy - move_y

        # If a click event is received, ignore any movement in the same packet
        if click == 1:
            residual_x = 0.0
            residual_y = 0.0
            pyautogui.click()
            sock.sendto("ACK\n".encode('utf-8'), addr)
            continue

        # Move the mouse cursor only when there is valid movement
        if move_x != 0 or move_y != 0:
            pyautogui.moveRel(move_x, move_y)

        # Apply scrolling if a scroll event is received
        if scroll != 0:
            pyautogui.scroll(int(scroll * SCROLL_MULTIPLIER))
            sock.sendto("ACK\n".encode('utf-8'), addr)

    except json.JSONDecodeError:
        pass
    except Exception as e:
        pass