import can
import csv
from datetime import datetime

def main():
    bus1 = can.interface.Bus(channel='test', interface='virtual')
    bus2 = can.interface.Bus(channel='test', interface='virtual')

    msg1 = can.Message(arbitration_id=0xabcde, data=[0, 25, 0, 1, 3, 1, 4, 0], is_extended_id=True)
    bus1.send(msg1)
    print("Message sent from bus1:", msg1)

    msg2 = bus2.recv()
    print("Message received on bus2:", msg2)


    msg3 = can.Message(arbitration_id=0x100, data=[1, 2, 3], is_extended_id=False, is_remote_frame=False)
    bus2.send(msg3)

    msg4 = bus1.recv()
    print("Message received on bus1:", msg4)

    msg5 = can.Message(arbitration_id=0x200, is_extended_id=False, is_remote_frame=True)
    bus2.send(msg5)
    print("Message sent from bus2 (remote):", msg5)

    msg6 = bus1.recv()
    print("Message received on bus1 (remote):", msg6)

    msg7 = can.Message(arbitration_id=0x7FF, data=[2, 4, 8, 9, 0], is_extended_id=False, is_error_frame=True)
    print("Error message frame:", msg7)

    bus1.shutdown()
    bus2.shutdown()

if __name__ == "__main__":
    main()
