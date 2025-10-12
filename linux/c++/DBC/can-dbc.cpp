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
#include <fstream>
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

map<unsigned int, CANMessageDef> dbc_map = {
    {0x100, {"EngineData", {
        {"EngineTemp", 0, 16, 0.01, 0.0, "°C"},
        {"BatteryVolt", 16, 16, 0.01, 0.0, "V"},
        {"RPM", 32, 32, 1.0, 0.0, "rpm"}
    }}}
};

void encodeSignals(can_frame &frame, float temp, float volt, int rpm) {
    frame.can_id = 0x100;
    frame.can_dlc = 8;
    memset(frame.data, 0, 8);

    int16_t temp_raw = (int16_t)((temp - 0.0f) / 0.01f);
    int16_t volt_raw = (int16_t)((volt - 0.0f) / 0.01f);
    int32_t rpm_raw  = (int32_t)((rpm - 0.0f) / 1.0f);

    frame.data[0] = temp_raw & 0xFF;
    frame.data[1] = (temp_raw >> 8) & 0xFF;
    frame.data[2] = volt_raw & 0xFF;
    frame.data[3] = (volt_raw >> 8) & 0xFF;
    frame.data[4] = rpm_raw & 0xFF;
    frame.data[5] = (rpm_raw >> 8) & 0xFF;
    frame.data[6] = (rpm_raw >> 16) & 0xFF;
    frame.data[7] = (rpm_raw >> 24) & 0xFF;
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

    can_frame frame{};
    srand(time(0));

    while (true) {
        float temp = 80.0f + (rand() % 500) / 10.0f;
        float volt = 12.0f + (rand() % 100) / 10.0f;
        int rpm = 800 + (rand() % 6000);

        encodeSignals(frame, temp, volt, rpm);
        write(s, &frame, sizeof(frame));

        this_thread::sleep_for(chrono::seconds(1));
    }

    close(s);
}

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

    cout << "[Receiver] Listening on " << ifname << "...\n";
    ofstream csv("can_dbc_log.csv");
    csv << "Timestamp,EngineTemp,BatteryVolt,RPM\n";

    can_frame frame{};
    while (true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("Receiver Read");
            break;
        }

        auto decoded = decodeFrame(frame);
        if (!decoded.empty()) {
            auto now = chrono::system_clock::now();
            time_t t = chrono::system_clock::to_time_t(now);
            tm tm = *localtime(&t);

            cout << put_time(&tm, "%H:%M:%S") << " "
                 << "Temp: " << fixed << setprecision(2) << decoded["EngineTemp"] << "°C, "
                 << "Volt: " << decoded["BatteryVolt"] << "V, "
                 << "RPM: " << decoded["RPM"] << endl;

            csv << put_time(&tm, "%H:%M:%S") << ","
                << decoded["EngineTemp"] << ","
                << decoded["BatteryVolt"] << ","
                << decoded["RPM"] << "\n";
            csv.flush();
        }
    }

    csv.close();
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
