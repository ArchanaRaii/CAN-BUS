# CAN-BUS Project

This project demonstrates **CAN bus communication** in Python and C++ using the python-can library.  
It supports both **simulation** (virtual CAN interfaces) and **real hardware** using the Waveshare USB-CAN-A adapter.

---

## Requirements
- Python 3.8+
- python-can
- Linux with socketcan (for simulation)
- Waveshare USB-CAN-A driver (for hardware mode)

Install dependencies:
pip install python-can

---

## Repository Structure
```bash
│── linux/
│   ├── can-sender.py              # Virtual sender node
│   ├── can-receiver.py            # Virtual receiver node
│   ├── can-callback-receiver.py   # Callback-based receiver
│
│── windows/
│   ├── can-virtual.py             # Python simulation (send/receive on vcan or COM)
│   ├── can-virtual.cpp            # C++ simulation of CAN messages
│   ├── can-logging.py             # Logging CAN traffic (simulation)
│   ├── usb_can_sender_receiver.py # Hardware mode (Waveshare USB-CAN-A)
│
└── README.md                      # Project documentation
```
---

## Simulation Mode (Linux)

1. Setup Virtual CAN Interface
```bash
sudo modprobe vcan
sudo ip link add vcan1 type vcan
sudo ip link set up vcan1
```

2. Run Sender and Receiver
### Terminal 1 (receiver)
```bash
python3 can-receiver.py
```

### Terminal 2 (sender)
```bash
python3 can-sender.py
```

3. Logging CAN Messages
```bash
candump vcan1 -l
```
This generates a log file like:
candump-2025-09-22_022331.log

4. Converting Log to CSV
```bash
awk 'BEGIN {OFS=","; print "Timestamp","Interface","CAN_ID","Data"} 
{
    ts=$1; gsub(/[()]/,"",ts);   # remove parentheses from timestamp
    iface=$2;
    split($3,a,"#");             # split CAN_ID and DATA
    id=a[1]; data=a[2];
    human_time = strftime("%Y-%m-%d %H:%M:%S", int(ts));  # convert to human-readable
    print human_time, iface, id, data
}' candump-2025-09-22_022331.log > can_log.csv
```
Now open can_log.csv in Excel or any spreadsheet tool.

---

## Hardware Mode (Windows with USB-CAN-A)

When using two Waveshare USB-CAN-A converters:

1. Connect both adapters to your laptop
2. Assign each adapter a COM port (via Windows Device Manager)
3. Update usb-can-sender-receiver.py with the correct COM port names
4. Run:
```bash
python usb-can-sender-receiver.py
```
This will simulate a sender and receiver communicating through real hardware.

---

## Notes
- Linux examples use vcan1 for virtual simulation.
- Windows examples require configuring the COM port of the USB-CAN-A adapter.
- Both simulation and hardware modes demonstrate sending, receiving, and logging CAN traffic.
