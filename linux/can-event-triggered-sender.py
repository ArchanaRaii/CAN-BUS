import can
import time
import random

dbc = {
    0x124: {   # Vehicle Status
        'VehicleSpeed': {'start_bit': 0, 'length': 16, 'endianess': 'little', 'factor': 0.01, 'offset': 0, 'unit': 'km/h'}
    }
}

LOWER_THRESHOLD = 20.0
UPPER_THRESHOLD = 22.0

def get_signal_value(data_bytes, start_bit, length, endianess='little', factor=0.01, offset=0.0):
    val = int.from_bytes(data_bytes, byteorder='little' if endianess=='little' else 'big')
    raw_val = (val >> start_bit) & ((1 << length) - 1)
    return raw_val * factor + offset

def main():
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")
    last_value = None
    start_time = time.time()

    try:
        while True:
            new_data = [random.randint(0, 255) for _ in range(8)]
            arbitration_ID = 0x124  

            if random.random() < 0.2:
                desired_val = random.uniform(LOWER_THRESHOLD, UPPER_THRESHOLD)
                raw_val = int(desired_val / dbc[arbitration_ID]['VehicleSpeed']['factor'])
                new_data[0:2] = raw_val.to_bytes(2, byteorder='little')


            props = dbc[arbitration_ID]['VehicleSpeed']
            signal_val = get_signal_value(new_data, props['start_bit'], props['length'],
                                          props['endianess'], props['factor'], props['offset'])

            if LOWER_THRESHOLD <= signal_val <= UPPER_THRESHOLD and signal_val != last_value:
                msg = can.Message(arbitration_id=arbitration_ID, data=new_data, is_extended_id=False)
                try:
                    bus.send(msg)
                    ts = time.time() - start_time
                    print(f"[{ts:.3f}s] Sensor2 sent data on ID=0x{arbitration_ID:X}")
                except can.CanError:
                    print("Send failed!")

                last_value = signal_val

            time.sleep(0.2)

    except KeyboardInterrupt:
        bus.shutdown()

if __name__ == "__main__":
    main()
