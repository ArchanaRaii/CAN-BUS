import can
import csv
import time
import matplotlib.pyplot as plt
from collections import deque


dbc = {
    0x123: {   # Powertrain Data
        'EngineSpeed': {'start_bit': 24, 'length': 16, 'endianess': 'little'},
        'ThrottlePosition': {'start_bit': 8, 'length': 8, 'endianess': 'little'},
        'CoolantTemp': {'start_bit': 0, 'length': 8, 'endianess': 'little'}
    },
    0x124: {   # Vehicle Status
        'VehicleSpeed': {'start_bit': 0, 'length': 16, 'endianess': 'little'},
        'FuelLevel': {'start_bit': 16, 'length': 8, 'endianess': 'little'},
        'Odometer': {'start_bit': 24, 'length': 32, 'endianess': 'little'}
    },
    0x200: {   # Sensor Data
        'OilPressure': {'start_bit': 0, 'length': 16, 'endianess': 'little'},
        'BatteryVoltage': {'start_bit': 16, 'length': 16, 'endianess': 'little'},
        'IntakeAirTemp': {'start_bit': 32, 'length': 8, 'endianess': 'little'}
    }
}

def extract_signal(raw_data, start_bit, length, endianess):
    start_byte = start_bit // 8
    end_byte = (start_bit + length - 1) // 8
    signal_bytes = raw_data[start_byte:end_byte + 1]

    if endianess == 'little':
        signal_bytes.reverse()

    value = 0
    for i, byte in enumerate(signal_bytes):
        value |= byte << (8 * i)
    return value

def decode_can_message(can_id, raw_data, dbc):
    decoded_signals = {}
    if can_id in dbc:
        for name, info in dbc[can_id].items():
            val = extract_signal(raw_data, info['start_bit'], info['length'], info['endianess'])
            decoded_signals[name] = val
    return decoded_signals

def receiver(csv_file="can_log.csv"):
    bus = can.interface.Bus(channel='vcan0', interface='socketcan')

    with open(csv_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Timestamp", "CAN_ID", "Signal", "Value"])

        # Live plot setup
        signals_to_plot = ["EngineSpeed", "VehicleSpeed", "FuelLevel"]
        data_buffers = {sig: deque(maxlen=50) for sig in signals_to_plot}

        plt.ion()
        fig, ax = plt.subplots()
        lines = {}
        for sig in signals_to_plot:
            (line,) = ax.plot([], [], label=sig)
            lines[sig] = line

        ax.set_ylim(0, 7000)  # adjust dynamically if needed
        ax.set_xlim(0, 50)
        ax.set_xlabel("Samples")
        ax.set_ylabel("Value")
        ax.legend()

        while True:
            msg = bus.recv()
            decoded = decode_can_message(msg.arbitration_id, msg.data, dbc)

            for sig, val in decoded.items():
                writer.writerow([time.time(), hex(msg.arbitration_id), sig, val])
                print(f"[{time.strftime('%H:%M:%S')}] {sig} = {val}")

                # Update live plot buffer
                if sig in data_buffers:
                    data_buffers[sig].append(val)
                    lines[sig].set_ydata(list(data_buffers[sig]))
                    lines[sig].set_xdata(range(len(data_buffers[sig])))

            # Update plot range dynamically
            max_y = max((max(buf) if buf else 0) for buf in data_buffers.values())
            ax.set_ylim(0, max(100, max_y + 50))
            ax.set_xlim(0, max(len(buf) for buf in data_buffers.values()))
            plt.pause(0.01)

if __name__ == "__main__":
    receiver()
