import matplotlib.pyplot as plt
import matplotlib.animation as animation
import csv
from datetime import datetime
import random

# Use built-in style
plt.style.use('ggplot')

# CSV file
CSV_FILE = "node_status_log.csv"

# Data lists
timestamps = []
ecu_TEC = []
ecu_REC = []
sensor_TEC = []
sensor_REC = []
dashboard_TEC = []
dashboard_REC = []

# Initialize CSV
with open(CSV_FILE, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(["Timestamp",
                     "ECU_TEC","ECU_REC",
                     "Sensor_TEC","Sensor_REC",
                     "Dashboard_TEC","Dashboard_REC"])

# Simulate error counters from C++ nodes (for demo purposes)
def read_node_counters():
    # Randomized for demo: simulate TEC/REC increasing
    return {
        "ECU": (random.randint(0,250), random.randint(0,120)),
        "Sensor": (random.randint(0,250), random.randint(0,120)),
        "Dashboard": (random.randint(0,250), random.randint(0,120))
    }

# Animation function
def animate(i):
    global timestamps, ecu_TEC, ecu_REC, sensor_TEC, sensor_REC, dashboard_TEC, dashboard_REC

    now = datetime.now().strftime("%H:%M:%S")
    counters = read_node_counters()

    timestamps.append(now)
    ecu_TEC.append(counters["ECU"][0])
    ecu_REC.append(counters["ECU"][1])
    sensor_TEC.append(counters["Sensor"][0])
    sensor_REC.append(counters["Sensor"][1])
    dashboard_TEC.append(counters["Dashboard"][0])
    dashboard_REC.append(counters["Dashboard"][1])

    # Write to CSV
    with open(CSV_FILE, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([now,
                         counters["ECU"][0], counters["ECU"][1],
                         counters["Sensor"][0], counters["Sensor"][1],
                         counters["Dashboard"][0], counters["Dashboard"][1]])

    plt.cla()  # Clear axis

    # Plot TEC
    plt.plot(timestamps, ecu_TEC, label="ECU TEC", color='r')
    plt.plot(timestamps, sensor_TEC, label="Sensor TEC", color='g')
    plt.plot(timestamps, dashboard_TEC, label="Dashboard TEC", color='b')

    # Plot REC
    plt.plot(timestamps, ecu_REC, '--', label="ECU REC", color='r', alpha=0.5)
    plt.plot(timestamps, sensor_REC, '--', label="Sensor REC", color='g', alpha=0.5)
    plt.plot(timestamps, dashboard_REC, '--', label="Dashboard REC", color='b', alpha=0.5)

    plt.xticks(rotation=45, ha='right')
    plt.xlabel("Time")
    plt.ylabel("Error Counters")
    plt.title("Live Node TEC/REC Status")
    plt.legend(loc='upper left')
    plt.tight_layout()

    # Limit to last 20 samples for clarity
    if len(timestamps) > 20:
        timestamps = timestamps[-20:]
        ecu_TEC = ecu_TEC[-20:]
        ecu_REC = ecu_REC[-20:]
        sensor_TEC = sensor_TEC[-20:]
        sensor_REC = sensor_REC[-20:]
        dashboard_TEC = dashboard_TEC[-20:]
        dashboard_REC = dashboard_REC[-20:]

# Animate plot
ani = animation.FuncAnimation(plt.gcf(), animate, interval=1000)
plt.show()
