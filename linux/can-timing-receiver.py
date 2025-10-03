import can
import time
import csv

dbc = {
    0x123: {   # Powertrain Data
        'EngineSpeed': {'start_bit': 24, 'length': 16, 'endianess': 'little', 'factor': 0.125, 'offset': 0, 'unit': 'RPM'},
        #'ThrottlePosition': {'start_bit': 8, 'length': 8, 'endianess': 'little', 'factor': 0.4, 'offset': 0, 'unit': '%'},
        #'CoolantTemp': {'start_bit': 0, 'length': 8, 'endianess': 'little', 'factor': 1, 'offset': -40, 'unit': '°C'}
    },
    0x124: {   # Vehicle Status
        'VehicleSpeed': {'start_bit': 0, 'length': 16, 'endianess': 'little', 'factor': 0.01, 'offset': 0, 'unit': 'km/h'},
        #'FuelLevel': {'start_bit': 16, 'length': 8, 'endianess': 'little', 'factor': 0.5, 'offset': 0, 'unit': '%'},
        #'Odometer': {'start_bit': 24, 'length': 32, 'endianess': 'little', 'factor': 1, 'offset': 0, 'unit': 'km'}
    }
}


def get_signal_value(data_bytes, start_bit, length, endianess='little'):
    val = int.from_bytes(data_bytes, byteorder='little' if endianess=='little' else 'big')
    return (val >> start_bit) & ((1 << length) - 1)

def main():
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")
    start_time = time.time()
    with open("can_timing_log.csv", 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Timestamp (s)", "Sensor","Message ID", "Signal Name", "Decoded Value", "Unit"])
        try:
            while True:
                msg = bus.recv()

                if msg.arbitration_id == 0x123:
                    sender = "Sensor1"
                elif msg.arbitration_id == 0x124:
                    sender = "Sensor2"

                if msg and msg.arbitration_id in dbc:
                    timestamp = time.time() - start_time
                    for signal_name, props in dbc[msg.arbitration_id].items():
                        raw_val = get_signal_value(msg.data, props['start_bit'], props['length'], props['endianess'])
                        phys_val = raw_val * props.get('factor',1) + props.get('offset',0)
                        unit = props.get('unit','')
                        print(f"t={timestamp:.3f}s {sender} ID=0x{msg.arbitration_id:X} {signal_name}: {phys_val} {unit}")
                        writer.writerow([f"{timestamp:.3f}", {sender}, f"0x{msg.arbitration_id:X}", signal_name, phys_val, unit])
        except KeyboardInterrupt:
            bus.shutdown()
                

if __name__ == "__main__":
    main()
