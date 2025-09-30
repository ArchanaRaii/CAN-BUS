import can
import time
import random

def main():
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")
    start_time = time.time()

    try:
        while True:
            random_data = [random.randint(0,255) for _ in range(8)]
            msg1 = can.Message(arbitration_id = 0x123, data = random_data, is_extended_id = False)
            msg2 = can.Message(arbitration_id = 0x124, data = random_data, is_extended_id = False)
            msg3 = can.Message(arbitration_id = 0x200, data = random_data, is_extended_id = False)

            for msg in [msg1, msg2, msg3]:
                try:
                    bus.send(msg)
                    print(f"Sent: ID={hex(msg.arbitration_id)} Data={msg.data.hex()}")
                except can.CanError:
                    print("Send failed!")

            time.sleep(1) 
    except KeyboardInterrupt:
        bus.shutdown()

if __name__ == "__main__":
    main()