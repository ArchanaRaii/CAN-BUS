import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

plt.style.use("seaborn-v0_8-darkgrid")

def animate(i):
    try:
        df = pd.read_csv("can_dbc_log.csv")

        if df.empty or not {"EngineTemp", "BatteryVolt", "RPM"}.issubset(df.columns):
            return

        plt.clf()
        plt.title("Live CAN Signal Plot (vs Samples)")
        plt.xlabel("Samples")
        plt.ylabel("Value")

        x = range(len(df))

        plt.plot(x, df["EngineTemp"], color="r", label="Temperature (°C)")
        plt.plot(x, df["BatteryVolt"], color="g", label="Voltage (V)")
        plt.plot(x, df["RPM"], color="b", label="RPM")

        plt.legend(loc="upper right")
        plt.tight_layout()
    except Exception:
        pass

ani = FuncAnimation(plt.gcf(), animate, interval=1000)
plt.show()
