import can
import time

BITRATE = 500_000  
INTERVAL = 1.0     

def calculate_bus_load(bits_sent, interval, bitrate):
    return (bits_sent / (bitrate * interval)) * 100.0

def main():
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")

    payload_bits = 0
    total_bits = 0
    frame_count = 0
    start_time = time.time()

    print(f"{'Time':>8} {'Frames':>6} {'PayloadBits':>12} {'TotalBits':>10} {'BusLoad%':>10}")

    try:
        while True:
            message = bus.recv(timeout=INTERVAL)
            if message:
                payload_bits += message.dlc * 8
                total_bits += message.dlc * 8 + 47
                frame_count += 1

            ts = time.time() - start_time
            if ts >= INTERVAL:
                bus_load = calculate_bus_load(total_bits, INTERVAL, BITRATE)
                print(f"{time.strftime('%H:%M:%S')} {frame_count:6} {payload_bits:12} {total_bits:10} {bus_load:10.2f}%")
                payload_bits = 0
                total_bits = 0
                frame_count = 0
                start_time = time.time()

    except KeyboardInterrupt:
        bus.shutdown()

if __name__ == "__main__":
    main()
