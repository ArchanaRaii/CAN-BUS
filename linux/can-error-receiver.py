import can
import csv
import time

msg_ID = 0x123
bus = can.interface.Bus(channel='vcan0', interface='socketcan')

def crc15_can(data_bytes):
    poly = 0x4599
    crc = 0x0000
    bits = ''.join(f"{b:08b}" for b in data_bytes)
    for bit in bits:
        bit = int(bit)
        msb = (crc >> 14) & 1
        crc = ((crc << 1) & 0x7FFF)
        if msb ^ bit:
            crc ^= poly
    return crc & 0x7FFF

expected_payload = b'\x11\x22\x33\x44\x55\x66'

bit_error_count = 0
crc_error_count = 0

with open("can_error_log.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["timestamp", "frame_id", "error_type"])
    start_time = time.time()

    try:
        while True:
            msg = bus.recv(timeout=1)
            if msg is None or len(msg.data) < 3:
                continue

            payload = msg.data[:-2]
            recv_crc = int.from_bytes(msg.data[-2:], 'big')
            calc_crc = crc15_can(payload)
            timestamp = time.time() - start_time

            if recv_crc != calc_crc:
                if payload != expected_payload:
                    error_type = "BIT_ERROR"
                    bit_error_count += 1
                else:
                    error_type = "CRC_ERROR"
                    crc_error_count += 1
            else:
                error_type = "OK"

            writer.writerow([timestamp, hex(msg.arbitration_id), error_type])

    except KeyboardInterrupt:
        bus.shutdown()

print(f"Bit errors: {bit_error_count}")
print(f"CRC errors: {crc_error_count}")
