import can
import csv
import time
import matplotlib.pyplot as plt
from collections import deque

dbc = {
    0x123: {   # Powertrain Data
        'EngineSpeed': {'start_bit': 24, 'length': 16, 'endianess': 'little', 'factor': 0.125, 'offset': 0, 'unit': 'RPM'},
        'ThrottlePosition': {'start_bit': 8, 'length': 8, 'endianess': 'little', 'factor': 0.4, 'offset': 0, 'unit': '%'},
        'CoolantTemp': {'start_bit': 0, 'length': 8, 'endianess': 'little', 'factor': 1, 'offset': -40, 'unit': '°C'}
    },
    0x124: {   # Vehicle Status
        'VehicleSpeed': {'start_bit': 0, 'length': 16, 'endianess': 'little', 'factor': 0.01, 'offset': 0, 'unit': 'km/h'},
        'FuelLevel': {'start_bit': 16, 'length': 8, 'endianess': 'little', 'factor': 0.5, 'offset': 0, 'unit': '%'},
        'Odometer': {'start_bit': 24, 'length': 32, 'endianess': 'little', 'factor': 1, 'offset': 0, 'unit': 'km'}
    },
    0x200: {   # Sensor Data
        'OilPressure': {'start_bit': 0, 'length': 16, 'endianess': 'little', 'factor': 0.1, 'offset': 0, 'unit': 'kPa'},
        'BatteryVoltage': {'start_bit': 16, 'length': 16, 'endianess': 'little', 'factor': 0.01, 'offset': 0, 'unit': 'V'},
        'IntakeAirTemp': {'start_bit': 32, 'length': 8, 'endianess': 'little', 'factor': 1, 'offset': -40, 'unit': '°C'}
    }
}


def get_signal_value(data_bytes, start_bit, length, endianess='little'):
    val = int.from_bytes(data_bytes, byteorder='little' if endianess == 'little' else 'big')
    return (val >> start_bit) & ((1 << length) - 1)

def receiver(csv_file="can_log.csv"):
    bus = can.interface.Bus(channel='vcan0', interface='socketcan')
    start_time = time.time()

    with open(csv_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Timestamp (s)", "Message ID", "Signal Name", "Value", "Unit"])

        # Live plot setup
        signals_to_plot = ["EngineSpeed", "VehicleSpeed", "CoolantTemp"]
        data_buffers = {sig: deque(maxlen=50) for sig in signals_to_plot}

        plt.ion()
        fig, ax = plt.subplots()
        lines = {}
        for sig in signals_to_plot:
            (line,) = ax.plot([], [], label=sig)
            lines[sig] = line

        ax.set_ylim(0, 7000)
        ax.set_xlim(0, 50)
        ax.set_xlabel("Samples")
        ax.set_ylabel("Value")
        ax.legend()

        while True:
            msg = bus.recv()
            timestamp = time.time() - start_time

            if msg.arbitration_id in dbc:
                for signal_name, props in dbc[msg.arbitration_id].items():
                    raw_val = get_signal_value(msg.data, props['start_bit'], props['length'], props['endianess'])
                    phys_val = raw_val * props.get('factor', 1) + props.get('offset', 0)
                    unit = props.get('unit', '')

                    print(f"t={timestamp:.3f}s ID=0x{msg.arbitration_id:X} "
                          f"{signal_name}: {phys_val} {unit}")
                    writer.writerow([f"{timestamp:.3f}", f"0x{msg.arbitration_id:X}",
                                     signal_name, phys_val, unit])

                    # Live plot update
                    if signal_name in data_buffers:
                        data_buffers[signal_name].append(phys_val)
                        lines[signal_name].set_ydata(list(data_buffers[signal_name]))
                        lines[signal_name].set_xdata(range(len(data_buffers[signal_name])))

            # Update plot scaling
            all_vals = [val for buf in data_buffers.values() for val in buf]
            if all_vals:
                ax.set_ylim(min(all_vals) - 10, max(all_vals) + 50)
            ax.set_xlim(0, max((len(buf) for buf in data_buffers.values()), default=50))
            plt.pause(0.01)

if __name__ == "__main__":
    receiver()
