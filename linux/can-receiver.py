import can
import time

def main():
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")
    start_time = time.time()
    print("Listening on vcan0...")
    while True:
        msg = bus.recv()
        if msg:
            ts = time.time() - start_time
            print(f"RX: Timestamp={ts:.6f} ID=0x{msg.arbitration_id:X}, Data={msg.data.hex()}")


if __name__ == "__main__":
    main()

