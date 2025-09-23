import can
import time

start_time = time.time()

# function will be called automatically whenever a CAN frame arrives
def on_message_received(msg):
    ts = time.time() - start_time
    print(
        f"RX (callback): Timestamp={ts:.6f}, "
        f"ID=0x{msg.arbitration_id:X}, "
        f"Data={[hex(b) for b in msg.data]}"
    )

def main():
    bus = can.interface.Bus(channel="vcan1", interface="socketcan")
    notifier = can.Notifier(bus, [on_message_received])

    print("Listening on vcan0 with callback...")
   
    try:
        while True:
            time.sleep(0.1)  # keep main thread alive
    except KeyboardInterrupt:
        print("\nStopping receiver...")
    finally:
        notifier.stop()
        bus.shutdown()

if __name__ == "__main__":
    main()
