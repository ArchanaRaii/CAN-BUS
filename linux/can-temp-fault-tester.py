import can
import random
import time

# CAN IDs
REQUEST_ID = 0x7DF  # Diagnostic request broadcast
RESPONSE_ID = 0x7E8  # ECU response


DTCs = {
    "P0128": "Coolant not reaching proper operating temp",
    "P0217": "Engine over-temperature"
}


TEMP_LOW = 70
TEMP_HIGH = 110

def simulate_temperature():
    return random.uniform(65, 120)

def generate_dtc(temp):
    if temp < TEMP_LOW:
        return "P0128"
    elif temp > TEMP_HIGH:
        return "P0217"
    else:
        return None

def handle_request(msg, bus, dtc_code):
    if msg.arbitration_id == REQUEST_ID:
        if dtc_code:
            data = bytearray(dtc_code.encode("ascii"))  # Simplified DTC payload
            response = can.Message(arbitration_id=RESPONSE_ID, data=data, is_extended_id=False)
            bus.send(response)
            print(f"Sent DTC response: {dtc_code}")

def main():
    bus = can.Bus(channel="vcan0", interface="socketcan")

    try:
        dtc_code = None
        while True:
            temp = simulate_temperature()
            dtc_code = generate_dtc(temp)
            print(f"Temperature: {temp:.1f}°C DTC: {dtc_code if dtc_code else 'None'}")

            msg = bus.recv(timeout=0.1)
            if msg:
                handle_request(msg, bus, dtc_code)

            time.sleep(1)

    except KeyboardInterrupt:
        bus.shutdown()

if __name__ == "__main__":
    main()
