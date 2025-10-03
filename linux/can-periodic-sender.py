import can
import time
import random

def main():
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")
    start_time = time.time()

    try:
        while True:
            arbitration_ID = 0x123
            random_data = [random.randint(0,255) for _ in range(8)]
            msg = can.Message(arbitration_id = arbitration_ID, data = random_data, is_extended_id = False)
            msg.timestamp = time.time() - start_time
            bus.send(msg)
            print(f"[{msg.timestamp:.3f}s] Sensor1 sent data on ID=0x{msg.arbitration_id:X}")
            time.sleep(0.01)
    except KeyboardInterrupt:
        bus.shutdown()

if __name__ == "__main__":
    main()