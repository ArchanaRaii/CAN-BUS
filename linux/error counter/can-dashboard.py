import can
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

SENDER_STATUS_ID = 0x301
RECEIVER_STATUS_ID = 0x302

WARNING_THRESHOLD = 128
FAIL_THRESHOLD = 255

# History buffers
tec_history = deque(maxlen=100)
rec_history = deque(maxlen=100)
time_history = deque(maxlen=100)

bus = can.Bus(channel="vcan0", interface="socketcan")

def get_status_color(value):
    if value < WARNING_THRESHOLD:
        return "green"
    elif value <= FAIL_THRESHOLD:
        return "orange"
    else:
        return "red"

def update(frame):
    # Read multiple messages quickly to avoid lag
    for _ in range(5):
        msg = bus.recv(timeout=0.001)
        if not msg:
            continue

        if msg.arbitration_id == SENDER_STATUS_ID:
            tec = int.from_bytes(msg.data, byteorder="big")
            tec_history.append(tec)
        elif msg.arbitration_id == RECEIVER_STATUS_ID:
            rec = int.from_bytes(msg.data, byteorder="big")
            rec_history.append(rec)

    time_history.append(frame)

    ax.clear()
    ax.set_title("Real-time CAN Node Error Counters (TEC/REC)")
    ax.set_xlabel("Samples")
    ax.set_ylabel("Error Counter")
    ax.set_ylim(0, 300)
    ax.grid(True)

    # Plot TEC with color-coded points
    for i, val in enumerate(tec_history):
        ax.scatter(i, val, color=get_status_color(val), s=30, label="Sender TEC" if i == 0 else "")

    # Plot REC
    for i, val in enumerate(rec_history):
        ax.scatter(i, val, color=get_status_color(val), marker="x", s=30, label="Receiver REC" if i == 0 else "")

    ax.legend(loc="upper left")

fig, ax = plt.subplots()
ani = animation.FuncAnimation(fig, update, interval=100)  # refresh every 100 ms
plt.show()
