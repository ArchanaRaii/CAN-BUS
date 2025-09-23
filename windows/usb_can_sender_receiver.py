import threading
import time
import csv
import can


def send_cyclic(bus, msg):
    print("Start to send a message every 1s")
    start_time = time.time()
    for i in range(5):
        msg.timestamp = time.time() - start_time
        bus.send(msg)
        print(f"tx: {msg}")
        time.sleep(1)
    print("Stopped sending messages")


def receive(bus):
    print("Start receiving messages")
    with open("can_log.csv", "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Timestamp", "CAN ID", "DLC", "Data"])
        for i in range(5):
            rx_msg = bus.recv(2)
            if rx_msg:
                timestamp = f"{rx_msg.timestamp:.6f}"
                can_id = f"{rx_msg.arbitration_id:08X}"
                dlc = rx_msg.dlc
                data_str = " ".join(f"{b:02X}" for b in rx_msg.data)
                print(f"rx: Timestamp: {timestamp}    ID: {can_id}    DL: {dlc}    Data: {data_str}")
                writer.writerow([timestamp, can_id, dlc, data_str])
    print("Stopped receiving messages")


def main():
    
    with can.Bus(interface="serial", channel="COM3") as server, \
         can.Bus(interface="serial", channel="COM4") as client:

        tx_msg = can.Message(
            arbitration_id=0x01,
            data=[0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88],
            is_extended_id=False,
        )

        t_send = threading.Thread(target=send_cyclic, args=(server, tx_msg))
        t_recv = threading.Thread(target=receive, args=(client,))

        t_recv.start()
        t_send.start()

        t_send.join()
        t_recv.join()

    print("Stopped script")


if __name__ == "__main__":
    main()
