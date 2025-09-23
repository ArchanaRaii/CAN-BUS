import can
import time
import random

def main():
        bus = can.interface.Bus(channel="vcan1", interface="socketcan")
        start_time = time.time()
        while True:
                random_ID = random.randint(0x200,0x300)
                random_data = [random.randint(0, 255) for _ in range(8)]
                msg = can.Message(arbitration_id=random_ID, data=random_data>
                msg.timestamp = time.time() - start_time
                bus.send(msg)
                print(f"Sent: {msg}")
                time.sleep(1)


if __name__ == "__main__":
        main()

