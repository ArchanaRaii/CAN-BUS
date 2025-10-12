#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

using namespace std;

struct Signal {
    string name;
    int start_bit;
    int length;
    float scale;
    float offset;
    string unit;
};

struct CANMessageDef {
    string name;
    vector<Signal> signals;
};

// DBC
map<unsigned int, CANMessageDef> dbc_map = {
    {0x100, {"EngineECU", {
        {"EngineTemp", 0, 16, 0.01, 0.0, "°C"},
        {"BatteryVolt", 16, 16, 0.01, 0.0, "V"},
        {"RPM", 32, 32, 1.0, 0.0, "rpm"}
    }}},
    {0x200, {"SensorCluster", {
        {"FuelLevel", 0, 16, 0.1, 0.0, "%"},
        {"CoolantPressure", 16, 16, 0.1, 0.0, "bar"}
    }}}
};

void encodeEngineECU(can_frame &frame, float temp, float volt, int rpm) {
    frame.can_id = 0x100;
    frame.can_dlc = 8;
    memset(frame.data, 0, 8);

    int16_t temp_raw = static_cast<int16_t>((temp - 0.0f) / 0.01f);
    int16_t volt_raw = static_cast<int16_t>((volt - 0.0f) / 0.01f);
    int32_t rpm_raw  = static_cast<int32_t>(rpm);

    frame.data[0] = temp_raw & 0xFF;
    frame.data[1] = (temp_raw >> 8) & 0xFF;
    frame.data[2] = volt_raw & 0xFF;
    frame.data[3] = (volt_raw >> 8) & 0xFF;
    frame.data[4] = rpm_raw & 0xFF;
    frame.data[5] = (rpm_raw >> 8) & 0xFF;
    frame.data[6] = (rpm_raw >> 16) & 0xFF;
    frame.data[7] = (rpm_raw >> 24) & 0xFF;
}

void encodeSensorCluster(can_frame &frame, float fuel, float coolant) {
    frame.can_id = 0x200;
    frame.can_dlc = 4;
    memset(frame.data, 0, 4);

    int16_t fuel_raw = static_cast<int16_t>(fuel / 0.1f);
    int16_t coolant_raw = static_cast<int16_t>(coolant / 0.1f);

    frame.data[0] = fuel_raw & 0xFF;
    frame.data[1] = (fuel_raw >> 8) & 0xFF;
    frame.data[2] = coolant_raw & 0xFF;
    frame.data[3] = (coolant_raw >> 8) & 0xFF;
}

map<string, float> decodeFrame(const can_frame &frame) {
    map<string, float> result;
    unsigned int can_id = frame.can_id & CAN_SFF_MASK;

    if (dbc_map.find(can_id) == dbc_map.end())
        return result;

    auto msgDef = dbc_map[can_id];

    for (auto &sig : msgDef.signals) {
        unsigned long raw_val = 0;
        int byte_start = sig.start_bit / 8;
        int byte_len = sig.length / 8;
        for (int i = 0; i < byte_len; i++)
            raw_val |= ((unsigned long)frame.data[byte_start + i] << (8 * i));
        float physical = raw_val * sig.scale + sig.offset;
        result[sig.name] = physical;
    }

    return result;
}

// Generic CAN Socket Setup
int setupCAN(const char *ifname) {
    int s;
    sockaddr_can addr{};
    ifreq ifr{};

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        exit(1);
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        exit(1);
    }
    return s;
}

// Engine ECU Sender
void engineECUThread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    srand(time(0));

    while (true) {
        float temp = 80.0f + (rand() % 500) / 10.0f;   // 80-130°C
        float volt = 12.0f + (rand() % 100) / 10.0f;  // 12-22 V
        int rpm = 800 + (rand() % 6000);              // 800-6800 rpm

        encodeEngineECU(frame, temp, volt, rpm);
        write(s, &frame, sizeof(frame));

        this_thread::sleep_for(chrono::seconds(1));
    }
    close(s);
}

// Sensor Cluster Sender
void sensorClusterThread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    srand(time(0));

    while (true) {
        float fuel = 10 + (rand() % 900) / 10.0f;         // 10-100 %
        float coolant = 1 + (rand() % 100) / 10.0f;       // 1-11 bar

        encodeSensorCluster(frame, fuel, coolant);
        write(s, &frame, sizeof(frame));

        this_thread::sleep_for(chrono::seconds(1));
    }
    close(s);
}

// Dashboard Receiver
void dashboardThread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    cout << "[Dashboard] Listening on " << ifname << "...\n";

    while (true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("Read");
            break;
        }

        auto decoded = decodeFrame(frame);
        if (!decoded.empty()) {
            cout << "Message ID: 0x" << hex << frame.can_id << dec << " | ";
            for (auto &[name, val] : decoded)
                cout << name << "=" << fixed << setprecision(2) << val << " ";
            cout << endl;
        }
    }
    close(s);
}

int main() {
    const char *ifname = "vcan0";

    thread engine(engineECUThread, ifname);
    thread sensors(sensorClusterThread, ifname);
    thread dashboard(dashboardThread, ifname);

    engine.join();
    sensors.join();
    dashboard.join();

    return 0;
}
