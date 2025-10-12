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

// DBC-style signal definition
struct Signal {
    string name;
    int start_bit;
    int length;
    float scale;
    float offset;
    string unit;
};

// Each CAN ID has multiple signals
struct CANMessageDef {
    string name;
    vector<Signal> signals;
};

// DBC database
map<unsigned int, CANMessageDef> dbc_map = {
    {0x100, {"EngineData", {
        {"EngineTemp", 0, 16, 0.01, 0.0, "°C"},
        {"BatteryVolt", 16, 16, 0.01, 0.0, "V"},
        {"RPM", 32, 32, 1.0, 0.0, "rpm"}
    }}}
};

void encodeSignals(struct can_frame &frame, float temp, float volt, int rpm) {
    frame.can_id = 0x100;
    frame.can_dlc = 8;
    memset(frame.data, 0, 8);

    // Apply DBC scaling & pack
    int16_t temp_raw = static_cast<int16_t>((temp - 0.0f) / 0.01f);
    int16_t volt_raw = static_cast<int16_t>((volt - 0.0f) / 0.01f);
    int32_t rpm_raw  = static_cast<int32_t>((rpm - 0.0f) / 1.0f);

    frame.data[0] = temp_raw & 0xFF;
    frame.data[1] = (temp_raw >> 8) & 0xFF;
    frame.data[2] = volt_raw & 0xFF;
    frame.data[3] = (volt_raw >> 8) & 0xFF;
    frame.data[4] = rpm_raw & 0xFF;
    frame.data[5] = (rpm_raw >> 8) & 0xFF;
    frame.data[6] = (rpm_raw >> 16) & 0xFF;
    frame.data[7] = (rpm_raw >> 24) & 0xFF;
}

// Decode dynamically using DBC map
void decodeFrame(const struct can_frame &frame) {
    unsigned int can_id = frame.can_id & CAN_SFF_MASK;

    if (dbc_map.find(can_id) == dbc_map.end()) {
        cout << "[Receiver] Unknown CAN ID 0x" << hex << uppercase << can_id << dec << endl;
        return;
    }

    auto msgDef = dbc_map[can_id];

    for (auto &sig : msgDef.signals) {
        // Extract raw value (little-endian)
        unsigned long raw_val = 0;
        int byte_start = sig.start_bit / 8;
        int byte_len = sig.length / 8;
        for (int i = 0; i < byte_len; i++)
            raw_val |= ((unsigned long)frame.data[byte_start + i] << (8 * i));

        // Apply scaling & offset
        float physical = raw_val * sig.scale + sig.offset;

        cout << "  " << setw(12) << left << sig.name << " : "
             << setw(8) << fixed << setprecision(2) << physical << " " << sig.unit << endl;
    }
}

// Sender thread
void senderThread(const char *ifname) {
    int s;
    sockaddr_can addr{};
    ifreq ifr{};

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Sender Socket");
        return;
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Sender Bind");
        return;
    }

    struct can_frame frame{};
    srand(time(0));

    while (true) {
        float temp = 80.0f + (rand() % 500) / 10.0f;   // 80.0–130.0 °C
        float volt = 12.0f + (rand() % 100) / 10.0f;   // 12.0–22.0 V
        int rpm = 800 + (rand() % 6000);               // 800–6800 rpm

        encodeSignals(frame, temp, volt, rpm);

        if (write(s, &frame, sizeof(frame)) != sizeof(frame))
            perror("Sender Write");

        this_thread::sleep_for(chrono::seconds(1)); 
    }

    close(s);
}

// Receiver thread
void receiverThread(const char *ifname) {
    int s;
    sockaddr_can addr{};
    ifreq ifr{};

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Receiver Socket");
        return;
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Receiver Bind");
        return;
    }

    cout << "[Receiver] Listening on " << ifname << "..." << endl;

    struct can_frame frame{};
    while (true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("Receiver Read");
            break;
        }

        decodeFrame(frame);
    }

    close(s);
}

int main() {
    const char *ifname = "vcan0";

    thread sender(senderThread, ifname);
    thread receiver(receiverThread, ifname);

    sender.join();
    receiver.join();

    return 0;
}
