import can
import time
import random

msg_ID = 0x123

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

# Flip a single bit in payload
def flip_bit(data):
    b = bytearray(data)
    b[0] ^= 0x01  # flip LSB of first byte
    return bytes(b)

def main():
    bus = can.interface.Bus(channel='vcan0', interface='socketcan')
    payload = b'\x11\x22\x33\x44\x55\x66'
    crc = crc15_can(payload)

    try:
        while True:
            if random.choice([True, False]):
                frame = flip_bit(payload) + crc.to_bytes(2,'big') 
            else:
                frame = payload + (crc ^ 0x7FFF).to_bytes(2,'big')  

            bus.send(can.Message(arbitration_id=msg_ID, data=frame, is_extended_id=False))
            time.sleep(0.05)  

    except KeyboardInterrupt:
        bus.shutdown()

if __name__ == "__main__":
    main()
