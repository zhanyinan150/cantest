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


# 接收累积缓冲: 跨多次 poll 保留未成帧的残留字节
_rx_acc = bytearray()


def poll_frames():
    """取走 UART 缓冲区里全部字节, 切出所有 XOR 校验通过的 8 字节帧。

    与 k230_competition.py 保持一致。原来的 `uart.read()` + `len>=8` 写法有两个硬伤:
      1) 读到半帧(如 3 字节)时整包丢弃, 那几个字节再也拿不回来, 后续永久错位;
      2) 读到两帧(16 字节)时只看前 8 个, 第二帧静默丢失 —— STM32 重试时必然发生。
    现在累积到 _rx_acc, 逐帧校验取出, 校验失败滑窗 1 字节重新找边界。

    注意: MicroPython 的 bytearray **不支持** `del ba[:n]` 切片删除
    (CPython 支持, 直接照搬会抛 "'bytearray' object doesn't support item
    deletion")。这里用游标 pos 标记已消费长度, 循环结束后整体重建一次。
    """
    global _rx_acc
    n = uart.any()
    if n:
        chunk = uart.read(n)
        if chunk:
            _rx_acc.extend(chunk)

    frames = []
    pos = 0                          # 已消费字节数
    total = len(_rx_acc)
    while total - pos >= FRAME_LEN:
        f = _rx_acc[pos:pos + FRAME_LEN]
        if verify_xor(f):
            frames.append(bytes(f))
            pos += FRAME_LEN
        else:
            pos += 1                 # 滑窗 1 字节, 重新寻找帧边界
    if pos:
        _rx_acc = bytearray(_rx_acc[pos:])
    if len(_rx_acc) > 64:
        _rx_acc = bytearray(_rx_acc[-16:])
    return frames


def flush_rx():
    """清空接收缓冲, 阶段切换时丢弃上一阶段的残留/重发帧"""
    global _rx_acc
    _rx_acc = bytearray()
    n = uart.any()
    if n:
        uart.read(n)


def wait_for_cmd(expected_cmd, timeout_ms=30000):
    start = time.ticks_ms()
    while True:
        for f in poll_frames():
            if f[0] == expected_cmd:
                return True
            print("  [WARN] discard 0x%02X, waiting 0x%02X" % (f[0], expected_cmd))
        # ticks_diff 处理计数回绕, 直接相减在 ticks_ms 翻转时会得到负数
        if timeout_ms > 0 and time.ticks_diff(time.ticks_ms(), start) > timeout_ms:
            return False
        time.sleep_ms(10)


# ======================== Mock 识别函数 ========================
BEAN_CODES = [0x06, 0x07, 0x08]  # g=绿豆, w=芸豆, y=黄豆
NUM_CODES = [0x01, 0x02, 0x03, 0x04, 0x05]  # 1-5


def pick(seq):
    """从 seq 里等概率取一个元素。

    不用 urandom.choice: 它属于 MicroPython 的可选编译项
    (MICROPY_PY_RANDOM_EXTRA_FUNCS), 不同固件不一定编进去 —— 和之前
    urandom.shuffle 根本不存在是同一类坑。getrandbits 是必定存在的。
    """
    n = len(seq)
    if n <= 1:
        return seq[0]
    bits = 1
    while (1 << bits) < n:
        bits += 1
    while True:                      # 拒绝采样, 保证均匀不偏
        v = urandom.getrandbits(bits)
        if v < n:
            return seq[v]


def mock_recognize_beans():
    """Mock: 随机生成3颗豆子颜色"""
    print("  [MOCK] >>> open camera 2, YOLO inference, anti-shake 3 frames <<<")
    time.sleep_ms(500)
    result = [pick(BEAN_CODES) for _ in range(3)]
    print("  [MOCK] Bean result: %s" % ["0x%02X" % b for b in result])
    return result


def mock_recognize_front_numbers():
    """Mock: 从1-5中随机选3个不重复数字"""
    print("  [MOCK] >>> open camera 1, YOLO inference, anti-shake 3 frames <<<")
    time.sleep_ms(500)
    nums = NUM_CODES[:]
    result = []
    for _ in range(3):
        n = pick(nums)
        nums.remove(n)
        result.append(n)
    print("  [MOCK] Front numbers: %s" % ["0x%02X" % b for b in result])
    return result


def mock_recognize_side_number(front_numbers):
    """Mock: 从剩余数字中随机选1个"""
    print("  [MOCK] >>> open camera 0, YOLO inference, anti-shake 3 frames <<<")
    time.sleep_ms(500)
    remaining = [n for n in NUM_CODES if n not in front_numbers]
    result = pick(remaining)
    print("  [MOCK] Side number: 0x%02X" % result)
    return result


def infer_fifth(known_nums):
    """与 k230_competition.py 保持一致: 保证总是返回合法数字(1~5)。

    原实现在已知 4 个数字不互异时返回 0x00, 而 STM32 端 K230.c 要求 0x01 帧的
    byte[2..6] 全部非 0, 会导致 full_number_flag 永不置位 -> 主机重试到超时放弃。
    """
    all_nums = [0x01, 0x02, 0x03, 0x04, 0x05]
    known_set = set(known_nums) - {0x00}
    missing = [n for n in all_nums if n not in known_set]

    if len(known_set) == 4 and len(missing) == 1:
        return missing[0]
    if missing:
        print("  [WARN] known numbers not distinct %s, guess 5th=0x%02X"
              % (["0x%02X" % n for n in known_nums], missing[0]))
        return missing[0]
    print("  [WARN] all 5 occupied, fallback 5th=0x01")
    return 0x01


# ======================== 主函数 ========================
if __name__ == "__main__":
    front_numbers = None

    try:
        print("========== K230 Mock Competition Start ==========")
        print("[MOCK] >>> load best.kmodel (skipped) <<<")
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
        print("  Bean data sent: %s" % ["0x%02X" % b for b in bean_result])

        if not wait_for_cmd(CMD_ACK_CLOSE, timeout_ms=10000):
            print("  Phase 1 ACK timeout!")
        print("  STM32 ACK received\n")

        # ============ Phase 2: 等待正面数字识别命令 ============
        # 丢弃上一阶段 STM32 重试遗留的帧, 否则会被误判成本阶段的命令
        flush_rx()
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
        # 丢弃上一阶段 STM32 重试遗留的帧, 否则会被误判成本阶段的命令
        flush_rx()
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
        print("  Known: %s" % ["0x%02X" % b for b in known])
        print("  Inferred 5th: 0x%02X" % fifth)
        print("  Full 5 numbers: %s" % ["0x%02X" % b for b in full_result])

        send_frame(CMD_START_SIDE, full_result)
        print("  Full number data sent")

        if not wait_for_cmd(CMD_ACK_CLOSE, timeout_ms=10000):
            print("  Phase 3 ACK timeout!")
        print("  STM32 ACK received\n")

        print("========== All phases complete ==========")

    except KeyboardInterrupt as e:
        print("User interrupted:", e)
    except BaseException as e:
        print("Exception: %s" % e)
    finally:
        print("[MOCK] >>> cleanup (skipped) <<<")
        gc.collect()
