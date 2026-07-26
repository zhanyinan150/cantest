"""
K230 比赛视觉系统 - 通讯测试版 (Mock)
=====================================
摄像头/AI推理用文字+随机数代替, 仅测试 STM32-K230 通讯协议。
STM32 主机 / K230 从机, USART3, 8字节定长帧, XOR校验, ACK应答。
"""

import os, sys, gc, time, urandom
from machine import FPIOA, UART

# ======================== 硬件配置 ========================
fpioa = FPIOA()
fpioa.set_function(50, FPIOA.UART3_TXD)
fpioa.set_function(51, FPIOA.UART3_RXD)

uart = UART(UART.UART3, baudrate=115200, bits=UART.EIGHTBITS,
            parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)

# ======================== 通信常量 ========================
FRAME_LEN = 8
CMD_START_BEAN = 0x02
CMD_START_FRONT = 0x03
CMD_START_SIDE = 0x01
CMD_ACK_CLOSE = 0x06
K230_ACK = 0x0A


# ======================== 通信辅助函数 ========================
def calc_xor(data):
    cs = 0
    for b in data:
        cs ^= b
    return cs


def verify_xor(frame):
    return calc_xor(frame[:7]) == frame[7]


def send_frame(frame_type, payload):
    frame = bytearray(FRAME_LEN)
    frame[0] = frame_type
    frame[1] = 0x00
    for i in range(min(len(payload), 5)):
        frame[2 + i] = payload[i]
    frame[7] = calc_xor(frame[:7])
    uart.write(frame)


def send_ack():
    frame = bytearray(FRAME_LEN)
    frame[0] = K230_ACK
    frame[7] = calc_xor(frame[:7])
    uart.write(frame)


def wait_for_cmd(expected_cmd, timeout_ms=30000):
    start = time.ticks_ms()
    while True:
        data = uart.read()
        if data and len(data) >= FRAME_LEN:
            if verify_xor(data[:FRAME_LEN]) and data[0] == expected_cmd:
                return True
            else:
                print(f"  [WARN] recv: {data[:FRAME_LEN].hex()}, expected 0x{expected_cmd:02X}")
        if timeout_ms > 0 and time.ticks_ms() - start > timeout_ms:
            return False
        time.sleep_ms(10)


# ======================== Mock 识别函数 ========================
BEAN_CODES = [0x06, 0x07, 0x08]  # g=绿豆, w=芸豆, y=黄豆
NUM_CODES = [0x01, 0x02, 0x03, 0x04, 0x05]  # 1-5


def mock_recognize_beans():
    """Mock: 随机生成3颗豆子颜色"""
    print("  [MOCK] >>> open camera 2, YOLO inference, anti-shake 3 frames <<<")
    time.sleep_ms(500)
    result = [urandom.choice(BEAN_CODES) for _ in range(3)]
    print(f"  [MOCK] Bean result: {['0x%02X' % b for b in result]}")
    return result


def mock_recognize_front_numbers():
    """Mock: 从1-5中随机选3个不重复数字"""
    print("  [MOCK] >>> open camera 1, YOLO inference, anti-shake 3 frames <<<")
    time.sleep_ms(500)
    nums = NUM_CODES[:]
    result = []
    for _ in range(3):
        n = urandom.choice(nums)
        nums.remove(n)
        result.append(n)
    print(f"  [MOCK] Front numbers: {['0x%02X' % b for b in result]}")
    return result


def mock_recognize_side_number(front_numbers):
    """Mock: 从剩余数字中随机选1个"""
    print("  [MOCK] >>> open camera 0, YOLO inference, anti-shake 3 frames <<<")
    time.sleep_ms(500)
    remaining = [n for n in NUM_CODES if n not in front_numbers]
    result = urandom.choice(remaining)
    print(f"  [MOCK] Side number: 0x{result:02X}")
    return result


def infer_fifth(known_nums):
    all_nums = {0x01, 0x02, 0x03, 0x04, 0x05}
    known_set = set(known_nums) - {0x00}
    if len(known_set) == 4:
        missing = all_nums - known_set
        return missing.pop()
    return 0x00


# ======================== 主函数 ========================
if __name__ == "__main__":
    front_numbers = None

    try:
        print("========== K230 Mock Competition Start ==========")
        print("[MOCK] >>> load bestm.kmodel (skipped) <<<")
        print("[MOCK] >>> Display.init (skipped) <<<")
        print("[MOCK] >>> MediaManager.init (skipped) <<<")
        time.sleep_ms(200)

        # ============ Phase 1: 等待豆子识别命令 ============
        print("\n========== Phase 1: Waiting for LOOK_BEAN (0x02) ==========")
        if not wait_for_cmd(CMD_START_BEAN, timeout_ms=30000):
            print("Phase 1 command timeout!")
            raise Exception("P1 timeout")
        print("  Command received, sending ACK")
        send_ack()

        print("  [MOCK] >>> open camera 2 <<<")
        bean_result = mock_recognize_beans()
        print("  [MOCK] >>> close camera 2 <<<")

        send_frame(CMD_START_BEAN, bean_result)
        print(f"  Bean data sent: {['0x%02X' % b for b in bean_result]}")

        if not wait_for_cmd(CMD_ACK_CLOSE, timeout_ms=10000):
            print("  Phase 1 ACK timeout!")
        print("  STM32 ACK received\n")

        # ============ Phase 2: 等待正面数字识别命令 ============
        print("========== Phase 2: Waiting for LOOK_NUMBER (0x03) ==========")
        if not wait_for_cmd(CMD_START_FRONT, timeout_ms=30000):
            print("Phase 2 command timeout!")
            raise Exception("P2 timeout")
        print("  Command received, sending ACK")
        send_ack()

        print("  [MOCK] >>> open camera 1 <<<")
        front_numbers = mock_recognize_front_numbers()
        print("  [MOCK] >>> close camera 1 <<<")

        send_ack()
        print("  Recognition done, ACK sent (no data frame)")

        if not wait_for_cmd(CMD_ACK_CLOSE, timeout_ms=10000):
            print("  Phase 2 ACK timeout!")
        print("  STM32 ACK received\n")

        # ============ Phase 3: 等待侧面数字识别命令 ============
        print("========== Phase 3: Waiting for LOOK_SIDE (0x01) ==========")
        if not wait_for_cmd(CMD_START_SIDE, timeout_ms=30000):
            print("Phase 3 command timeout!")
            raise Exception("P3 timeout")
        print("  Command received, sending ACK")
        send_ack()

        print("  [MOCK] >>> open camera 0 <<<")
        side_number = mock_recognize_side_number(front_numbers)
        print("  [MOCK] >>> close camera 0 <<<")

        known = [side_number] + front_numbers
        fifth = infer_fifth(known)
        full_result = known + [fifth]
        print(f"  Known: {['0x%02X' % b for b in known]}")
        print(f"  Inferred 5th: 0x{fifth:02X}")
        print(f"  Full 5 numbers: {['0x%02X' % b for b in full_result]}")

        send_frame(CMD_START_SIDE, full_result)
        print("  Full number data sent")

        if not wait_for_cmd(CMD_ACK_CLOSE, timeout_ms=10000):
            print("  Phase 3 ACK timeout!")
        print("  STM32 ACK received\n")

        print("========== All phases complete ==========")

    except KeyboardInterrupt as e:
        print("User interrupted:", e)
    except BaseException as e:
        print(f"Exception: {e}")
    finally:
        print("[MOCK] >>> cleanup (skipped) <<<")
        gc.collect()
