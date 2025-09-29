import can
import time
import random

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

def encode_can_message(can_id, signals, dbc):
    data = bytearray(8)
    if can_id in dbc:
        for name, val in signals.items():
            if name not in dbc[can_id]:
                continue
            info = dbc[can_id][name]
            start_byte = info['start_bit'] // 8
            length = info['length']

            for i in range((length + 7) // 8):
                data[start_byte + i] = (val >> (8 * i)) & 0xFF

    return can.Message(arbitration_id=can_id, data=data, is_extended_id=False)

def sender():
    bus = can.interface.Bus(channel='vcan0', interface='socketcan')

    engine_speed = 1000
    throttle = 10
    coolant = 70
    vehicle_speed = 0
    fuel_level = 80
    odometer = 12000
    oil_pressure = 200
    battery_voltage = 12000
    intake_air_temp = 30

    while True:
       #values changing
        engine_speed = (engine_speed + 150) % 7000
        throttle = (throttle + 2) % 100
        coolant = 70 + random.randint(-2, 2)
        vehicle_speed = (vehicle_speed + 5) % 200
        fuel_level = max(0, fuel_level - 1)
        odometer += 1
        oil_pressure = 200 + random.randint(-10, 10)
        battery_voltage = 12000 + random.randint(-100, 100)
        intake_air_temp = 25 + random.randint(-5, 5)


        msg1 = encode_can_message(0x123, {
            'EngineSpeed': engine_speed,
            'ThrottlePosition': throttle,
            'CoolantTemp': coolant
        }, dbc)

        msg2 = encode_can_message(0x124, {
            'VehicleSpeed': vehicle_speed,
            'FuelLevel': fuel_level,
            'Odometer': odometer
        }, dbc)

        msg3 = encode_can_message(0x200, {
            'OilPressure': oil_pressure,
            'BatteryVoltage': battery_voltage,
            'IntakeAirTemp': intake_air_temp
        }, dbc)

        for msg in [msg1, msg2, msg3]:
            try:
                bus.send(msg)
                print(f"Sent: ID={hex(msg.arbitration_id)} Data={msg.data.hex()}")
            except can.CanError:
                print("Send failed!")

        time.sleep(1) 

if __name__ == "__main__":
    sender()
