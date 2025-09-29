import can
import time
import random

def main():
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")
    start_time = time.time()
    arbitration_ID = 0x12
    role = "NODE A"

    try:
        while True:
            random_data = [random.randint(0,255) for _ in range(8)]
            msg = can.Message(arbitration_id = arbitration_ID, data = random_data, is_extended_id = False)
            msg.timestamp = time.time() - start_time
            bus.send(msg)
            print(f"{role} 0x{msg.arbitration_id:X} sent data packet...")
            time.sleep(1)
    except KeyboardInterrupt:
        bus.shutdown()

if __name__ == "__main__":
    main()
