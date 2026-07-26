from machine import FPIOA, UART
import time

fpioa = FPIOA()
fpioa.set_function(50, FPIOA.UART3_TXD)
fpioa.set_function(51, FPIOA.UART3_RXD)

uart = UART(UART.UART3, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)


def frame_checksum(frame):
    cs = 0
    for b in frame[:7]:
        cs ^= b
    return cs


def make_frame(frame_type, data_bytes):
    frame = bytearray(8)
    frame[0] = frame_type
    for i in range(min(5, len(data_bytes))):
        frame[2 + i] = data_bytes[i]
    frame[7] = frame_checksum(frame)
    return frame


def check_command(data, cmd_byte):
    if data and len(data) == 8:
        cs = 0
        for b in data[:7]:
            cs ^= b
        if cs == data[7] and data[0] == cmd_byte:
            return True
    return False


def wait_ack(timeout_ms=5000):
    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < timeout_ms:
        data = uart.read()
        if data:
            print(f"  RX: {data.hex()}")
            if check_command(data, 0x06):
                return True
        time.sleep_ms(10)
    return False


def wait_cmd(cmd_byte, timeout_ms=10000):
    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < timeout_ms:
        data = uart.read()
        if data:
            print(f"  RX: {data.hex()}")
            if check_command(data, cmd_byte):
                return True
        time.sleep_ms(10)
    return False


print("=== K230 Protocol Test (fake data) ===")

MAX_RETRY = 10

# P1: send bean frame (retry until ACK)
for attempt in range(MAX_RETRY):
    print(f"[P1] Send bean (try {attempt + 1})")
    frame = make_frame(0x02, [0x06, 0x07, 0x08])
    uart.write(frame)
    print(f"  TX: {frame.hex()}")

    if wait_ack(2000):
        print("[P1] ACK OK")
        break
else:
    print("[P1] ACK failed after retries!")
    while True:
        time.sleep_ms(1000)

# P2: wait for LOOK_NUMBER (0x03), retry P1 if timeout
while True:
    print("[P2] Wait LOOK_NUMBER...")
    if wait_cmd(0x03, 5000):
        print("[P2] Got LOOK_NUMBER")
        break
    print("[P2] Timeout, resend bean...")
    frame = make_frame(0x02, [0x06, 0x07, 0x08])
    uart.write(frame)
    print(f"  TX: {frame.hex()}")
    wait_ack(2000)

# P2: send front number frame (retry until ACK)
for attempt in range(MAX_RETRY):
    print(f"[P2] Send front (try {attempt + 1})")
    frame = make_frame(0x03, [0x01, 0x02, 0x03])
    uart.write(frame)
    print(f"  TX: {frame.hex()}")

    if wait_ack(2000):
        print("[P2] ACK OK")
        break
else:
    print("[P2] ACK failed after retries!")
    while True:
        time.sleep_ms(1000)

# P3: send full 5-digit frame (retry until ACK)
for attempt in range(MAX_RETRY):
    print(f"[P3] Send full (try {attempt + 1})")
    frame = make_frame(0x01, [0x01, 0x02, 0x03, 0x04, 0x05])
    uart.write(frame)
    print(f"  TX: {frame.hex()}")

    if wait_ack(2000):
        print("[P3] ACK OK")
        break
else:
    print("[P3] ACK failed after retries!")
    while True:
        time.sleep_ms(1000)

print("=== Protocol Test Done ===")

