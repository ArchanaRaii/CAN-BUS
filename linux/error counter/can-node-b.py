import can
import time
import random

class ReceiverECU:
    def __init__(self, channel="vcan0"):
        self.bus = can.Bus(channel=channel, interface="socketcan")
        self.REC = 0
        self.bus_off = False

    def get_status(self):
        if self.REC < 127:
            return "OK"
        elif self.REC <= 255:
            return "WARNING"
        else:
            return "FAIL"

    def receive_message(self):
        if self.bus_off:
            print("BUS OFF! Sleeping 2s...")
            time.sleep(2)
            self.REC = 0
            self.bus_off = False
            print("Recovered. Status=OK")
            return

        msg = self.bus.recv(timeout=0.01)
        if msg:
            if random.random() < 0.35:
                self.REC += 1
                print(f"REC={self.REC} Status={self.get_status()}")
            else:
                self.REC = max(0, self.REC - 1)

        data_bytes = self.REC.to_bytes(2, byteorder='big')
        status_msg = can.Message(arbitration_id=0x302, data=data_bytes, is_extended_id=False)

        try:
            self.bus.send(status_msg)
        except can.CanError:
            pass

        if self.REC > 255:
            self.bus_off = True

    def run(self):
        try:
            while True:
                self.receive_message()
                time.sleep(0.1)
        except KeyboardInterrupt:
            self.bus.shutdown()

if __name__ == "__main__":
    receiver = ReceiverECU()
    receiver.run()
