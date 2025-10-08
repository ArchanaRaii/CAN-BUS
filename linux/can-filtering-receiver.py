import can
import csv
import time

def main():

    with open("can_filter_log.csv", mode="w", newline="") as csvfile:
        fieldnames = ["timestamp", "can_id", "dlc", "data"]
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        bus = can.Bus(channel='vcan0', interface='socketcan',
                    can_filters=[{"can_id": 1, "can_mask": 0x7FF, "extended": False}])

        try:
            while True:
                msg = bus.recv(timeout=1)
                if msg is not None:
                    timestamp = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(msg.timestamp))
                    print(f"Processed message: ID={hex(msg.arbitration_id)}, data={msg.data.hex()}")

                    writer.writerow({
                        "timestamp": timestamp,
                        "can_id": hex(msg.arbitration_id),
                        "dlc": msg.dlc,
                        "data": msg.data.hex()
                    })
                    csvfile.flush()  
        except KeyboardInterrupt:
            print("\nReceiver stopped.")
        finally:
            bus.shutdown()

if __name__ == "__main__":
    main()
