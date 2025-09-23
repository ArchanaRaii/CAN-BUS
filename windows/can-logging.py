import can
import csv
from datetime import datetime
import time

def log_message(msg, writer):
    timestamp = datetime.fromtimestamp(msg.timestamp).strftime('%Y-%m-%d %H:%M:%S')
    can_id = hex(msg.arbitration_id)
    dlc = msg.dlc
    data = ' '.join(f'{byte:02X}' for byte in msg.data)
    writer.writerow([timestamp, can_id, dlc, data])

def main():
    bus1 = can.interface.Bus(channel='test', interface='virtual')
    bus2 = can.interface.Bus(channel='test', interface='virtual')

    csv_file = 'can_log.csv'
    file = open(csv_file, mode='w', newline='')
    writer = csv.writer(file)
    writer.writerow(['Timestamp', 'CAN ID', 'DLC', 'Data'])

    msg1 = can.Message(arbitration_id=0xabcde, data=[0, 25, 0, 1, 3, 1, 4, 0], is_extended_id=True)
    bus1.send(msg1)
    print("Message sent from bus1 ", msg1)
    time.sleep(2)

    msg2 = bus2.recv()
    print("Message received on bus2 ", msg2)
    log_message(msg2, writer)
    time.sleep(2)

    msg3 = can.Message(arbitration_id=0x100, data=[1, 2, 3], is_extended_id=False, is_remote_frame=False)
    bus2.send(msg3)
    print("Message sent from bus2 ", msg3)
    time.sleep(2)

    msg4 = bus1.recv()
    print("Message received on bus1 ", msg4)
    log_message(msg4, writer)
    time.sleep(2)

    msg5 = can.Message(arbitration_id=0x200, is_extended_id=False, is_remote_frame=True)
    bus2.send(msg5)
    print("Message sent from bus2(remote)", msg5)
    log_message(msg5, writer)
    time.sleep(2)

    msg6 = bus1.recv()
    print("Message received on bus1(remote)", msg6)
    log_message(msg6, writer)
    time.sleep(2)

    msg7 = can.Message(arbitration_id=0x7FF, data=[2, 4, 8, 9, 0], is_extended_id=False, is_error_frame=True)
    print("Error message frame ", msg7)
    log_message(msg7, writer)
    time.sleep(2)

    file.close()
    bus1.shutdown()
    bus2.shutdown()
    print("CAN logging completed.")

if __name__ == "__main__":
    main()
