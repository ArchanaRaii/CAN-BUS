import can
import time
import threading
import matplotlib.pyplot as plt
from collections import deque
import matplotlib.animation as animation
import numpy as np
from matplotlib.patches import FancyBboxPatch

BUS = "vcan0"
running = True

maxlen = 5000
t_buf = deque(maxlen=maxlen)
rpm_buf = deque(maxlen=maxlen)
temp_buf = deque(maxlen=maxlen)
fault_events = deque(maxlen=maxlen)
current_status = {"state": "Normal", "desc": "No active faults"}

start_time = time.monotonic()
last_event_time = 0
event_hold_duration = 10  # seconds to hold fault/recovery display

def decode_frame(msg):
    aid = msg.arbitration_id
    data = bytes(msg.data)
    rpm, temp, fault_state, desc = None, None, None, None

    if aid == 0x100 and msg.dlc >= 4:
        rpm = data[0] | (data[1] << 8)
        temp = data[3]

    elif aid in [0x7E8, 0x7E9, 0x7EA] and msg.dlc >= 2:
        if data[0] == 0x59 and data[2] != 0:
            fault_state = "ACTIVE"
            dtc_code = f"P{data[2]:02X}{data[3]:02X}"
            if dtc_code == "P0217":
                desc = "Engine Overheat (P0217)"
            elif dtc_code == "P0700":
                desc = "Transmission Fault (P0700)"
            elif dtc_code in ["PC123", "UC123", "C123"]:
                desc = "ABS Communication Fault (U0123)"
            else:
                desc = f"Unknown Fault ({dtc_code})"
        elif data[0] == 0x54:
            fault_state = "CLEARED"
            desc = "Fault Cleared"

    return rpm, temp, fault_state, desc


# CAN Listener Thread 
def can_listener():
    global current_status, last_event_time
    bus = can.interface.Bus(channel=BUS, interface="socketcan")
    while running:
        msg = bus.recv(timeout=1.0)
        if msg is None:
            continue
        rpm, temp, fault_state, desc = decode_frame(msg)
        t_rel = time.monotonic() - start_time

        if rpm is not None:
            t_buf.append(t_rel)
            rpm_buf.append(rpm)
        if temp is not None:
            temp_buf.append(temp)

        if fault_state:
            fault_events.append((t_rel, fault_state, desc))
            last_event_time = time.monotonic()  # mark event time
            if fault_state == "ACTIVE":
                current_status = {"state": "Fault", "desc": desc}
            elif fault_state == "CLEARED":
                current_status = {"state": "Recovery", "desc": desc}
    bus.shutdown()



plt.style.use("seaborn-v0_8-darkgrid")
fig, ax1 = plt.subplots(figsize=(11, 6))

line_rpm, = ax1.plot([], [], color="tab:blue", label="Engine RPM", linewidth=1.8)
line_temp, = ax1.plot([], [], color="tab:orange", label="Engine Temp (°C)", linewidth=1.5, linestyle="--")

ax1.set_ylabel("Value (RPM / °C)")
ax1.set_xlabel("Time (s since simulation start)")
ax1.set_ylim(0, 2000)
ax1.set_xlim(0, 20)
ax1.set_title("Real-Time Engine Diagnostics (RPM, Temperature & Fault Events)",
              fontsize=13, fontweight='bold')
ax1.legend(loc="upper right", fontsize=9)

#Dynamic Status Box
status_text = plt.figtext(
    0.02, 0.02,
    "NORMAL — System stable",
    fontsize=10,
    color="white",
    fontweight="bold",
    ha="left",
    va="bottom",
    bbox=dict(facecolor="#2E7D32", alpha=0.95, edgecolor="black", boxstyle="round,pad=0.4")
)

fault_lines = []

-
def on_scroll(event):
    if not t_buf:
        return
    cur_xlim = ax1.get_xlim()
    cur_xrange = cur_xlim[1] - cur_xlim[0]
    xdata = event.xdata if event.xdata else (cur_xlim[0] + cur_xlim[1]) / 2
    scale_factor = 1.2 if event.button == "up" else 0.8
    new_width = cur_xrange * scale_factor
    ax1.set_xlim(xdata - new_width / 2, xdata + new_width / 2)
    plt.draw()

def on_press(event):
    if event.button == 1:
        ax1._pan_start = event.xdata

def on_release(event):
    if hasattr(ax1, "_pan_start"):
        del ax1._pan_start

def on_motion(event):
    if hasattr(ax1, "_pan_start") and event.xdata is not None:
        dx = event.xdata - ax1._pan_start
        cur_xlim = ax1.get_xlim()
        ax1.set_xlim(cur_xlim[0] - dx, cur_xlim[1] - dx)
        ax1._pan_start = event.xdata
        plt.draw()

fig.canvas.mpl_connect("scroll_event", on_scroll)
fig.canvas.mpl_connect("button_press_event", on_press)
fig.canvas.mpl_connect("button_release_event", on_release)
fig.canvas.mpl_connect("motion_notify_event", on_motion)


# Update Plot 
def update_plot(frame):
    global current_status

    if t_buf:
        t_vals = list(t_buf)
        line_rpm.set_data(t_vals, list(rpm_buf))
        line_temp.set_data(t_vals, list(temp_buf))
        ax1.set_xlim(max(0, t_vals[-1] - 20), t_vals[-1] + 1)

    while fault_lines:
        line = fault_lines.pop(0)
        line.remove()

    used_positions = {}
    for (t, state, desc) in list(fault_events)[-10:]:
        x_group = round(t, 1)
        if state == "ACTIVE":
            base_y, color, style, off_y = 1700, "red", "--", 60
        else:
            base_y, color, style, off_y = 300, "green", ":", 40
        offset = used_positions.get(x_group, 0)
        used_positions[x_group] = offset + 1
        y_pos = base_y + offset * off_y
        vline = ax1.axvline(x=t, color=color, linestyle=style, alpha=0.6)
        fault_lines.append(vline)
        ax1.annotate(desc, xy=(t, y_pos), xytext=(t + 0.1, y_pos + 40),
                     fontsize=8, color=color,
                     arrowprops=dict(arrowstyle="->", color=color, lw=0.8))

    time_since_event = time.monotonic() - last_event_time
    state = current_status["state"]
    desc = current_status["desc"]

    if state == "Fault":
        status_text.set_text(f"FAULT DETECTED — {desc}")
        status_text.set_bbox(dict(facecolor="#C62828", edgecolor="black", alpha=0.95, boxstyle="round,pad=0.4"))
    elif state == "Recovery":
        status_text.set_text(f"RECOVERY — {desc}")
        status_text.set_bbox(dict(facecolor="#FFB300", edgecolor="black", alpha=0.95, boxstyle="round,pad=0.4"))
    elif time_since_event > event_hold_duration:
        status_text.set_text("NORMAL — System stable")
        status_text.set_bbox(dict(facecolor="#2E7D32", edgecolor="black", alpha=0.95, boxstyle="round,pad=0.4"))

    return line_rpm, line_temp, status_text


# Run Animation
listener_thread = threading.Thread(target=can_listener, daemon=True)
listener_thread.start()

ani = animation.FuncAnimation(fig, update_plot, interval=300, cache_frame_data=False)

try:
    plt.tight_layout(rect=[0, 0.05, 1, 1])
    plt.show()
except KeyboardInterrupt:
    pass
finally:
    running = False
    print("Live plot stopped.")
