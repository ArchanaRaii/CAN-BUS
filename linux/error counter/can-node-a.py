import can
import time
import random

class SenderECU:
    def __init__(self, channel="vcan0"):
        self.bus = can.Bus(channel=channel, interface="socketcan")
        self.TEC = 0
        self.bus_off = False

    def get_status(self):
        if self.TEC < 127:
            return "OK"
        elif self.TEC <= 255:
            return "WARNING"
        else:
            return "FAIL"

    def send_message(self):
        if self.bus_off:
            print("BUS OFF! Sleeping 2s...")
            time.sleep(2)
            self.TEC = 0
            self.bus_off = False
            print("Recovered. Status=OK")
            return

        msg = can.Message(arbitration_id=0x101 ,data=[random.randint(0, 255) for _ in range(8)], is_extended_id=False)

        if random.random() < 0.35: # 35% of error
            self.TEC += 8
            print(f"TEC={self.TEC} Status={self.get_status()}")
        else:
            try:
                self.bus.send(msg)
                self.TEC = max(0, self.TEC - 1)
            except can.CanError:
                self.TEC += 8
                print(f"TEC={self.TEC} Status={self.get_status()}")

        data_bytes = self.TEC.to_bytes(2, byteorder='big')
        status_msg = can.Message(arbitration_id=0x301, data=data_bytes, is_extended_id=False)

        try:
            self.bus.send(status_msg)
        except can.CanError:
            pass

        if self.TEC > 255:
            self.bus_off = True

    def run(self):
        try:
            while True:
                self.send_message()
                time.sleep(0.1)
        except KeyboardInterrupt:
            self.bus.shutdown()

if __name__ == "__main__":
    sender = SenderECU()
    sender.run()
