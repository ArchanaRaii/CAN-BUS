#include <iostream>
#include <thread>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

//Helpers
string time_local_now() {
    using namespace chrono;
    auto now = system_clock::now();
    auto tt = system_clock::to_time_t(now);
    auto us = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;
    tm local_tm {}; localtime_r(&tt, &local_tm);
    stringstream ss;
    ss << put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
       << "." << setw(6) << setfill('0') << us.count();
    return ss.str();
}

string node_name(uint32_t id) {
    switch (id) {
        case 0x100: return "Engine";
        case 0x120: return "Transmission";
        case 0x200: return "ABS";
        case 0x7E0: return "DiagTester->Engine";
        case 0x7E1: return "DiagTester->Trans";
        case 0x7E2: return "DiagTester->ABS";
        case 0x7E8: return "Engine(DiagResp)";
        case 0x7E9: return "Trans(DiagResp)";
        case 0x7EA: return "ABS(DiagResp)";
        default:    return "Unknown";
    }
}

string data_to_hex(const struct can_frame &f) {
    stringstream ss;
    ss << hex << setfill('0');
    for (int i = 0; i < f.can_dlc; ++i)
        ss << setw(2) << (int)f.data[i];
    return ss.str();
}

//Decode DTC
string decode_dtc(uint8_t a, uint8_t b) {
    if (a == 0 && b == 0) return "None";
    char dtc_letter;
    switch (a >> 6) {
        case 0b00: dtc_letter = 'P'; break;
        case 0b01: dtc_letter = 'C'; break;
        case 0b10: dtc_letter = 'B'; break;
        default:   dtc_letter = 'U'; break;
    }
    int code_num = ((a & 0x3F) << 8) | b;
    char formatted[10];
    snprintf(formatted, sizeof(formatted), "%c%04X", dtc_letter, code_num);
    return formatted;
}

//Decode CAN Frame
string decode_frame(const struct can_frame &f, int &rpm, int &temp, int &gear, int &ws, string &dtc, string &desc) {
    uint32_t id = f.can_id & CAN_SFF_MASK;

    static unordered_map<string, string> dtc_description = {
        {"P0217", "Engine Overheat"},
        {"P0700", "Transmission System Fault"},
        {"C1234", "ABS Wheel Speed Sensor Fault"},
        {"P0000", "No Fault Detected"},
        {"U1000", "CAN Communication Fault"}
    };

    if (id == 0x100) {  // Engine
        rpm = f.data[0] | (f.data[1] << 8);
        temp = f.data[3];
    } 
    else if (id == 0x120) {  // Transmission
        gear = f.data[0];
    }
    else if (id == 0x200) {  // ABS
        ws = f.data[0] | (f.data[1] << 8);
    }
    else if (id >= 0x7E8 && id <= 0x7EA) {  // Diagnostic responses
        if (f.data[0] == 0x59 && f.data[2] != 0) {
            string code = decode_dtc(f.data[2], f.data[3]);
            dtc = "Active:" + code;
            desc = dtc_description.count(code) ? dtc_description[code] : "Unknown DTC";
        } 
        else if (f.data[0] == 0x54) {
            dtc = "Cleared";
            desc = "DTC Cleared";
        }
        else {
            dtc = "None";
            desc = "No Active DTC";
        }
    }

    stringstream ss;
    ss << "RPM=" << rpm << ",Temp=" << temp << "C,Gear=" << gear 
       << ",WS=" << ws << ",DTC=" << dtc << ",Desc=" << desc;
    return ss.str();
}

int main() {
    string iface = "vcan0";
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    struct ifreq ifr {};
    strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
    ioctl(s, SIOCGIFINDEX, &ifr);
    struct sockaddr_can addr {}; 
    addr.can_family = AF_CAN; 
    addr.can_ifindex = ifr.ifr_ifindex;
    bind(s, (struct sockaddr*)&addr, sizeof(addr));

    ofstream log("vehicle_decoded_log.csv");
    log << "time_local,ts_mono,bus,can_id,dlc,data_hex,node_inferred,decoded_values\n";

    cout << "Logger started on " << iface << " (Press Ctrl+C to stop)" << endl;

    struct can_frame f {};
    auto start = chrono::steady_clock::now();
    int rpm = 0, temp = 0, gear = 0, ws = 0;
    string dtc = "None", desc = "No Active DTC";

    while (true) {
        ssize_t n = read(s, &f, sizeof(f));
        if (n > 0) {
            auto now = chrono::steady_clock::now();
            double ts = chrono::duration<double>(now - start).count();
            string node = node_name(f.can_id & CAN_SFF_MASK);
            string data_hex = data_to_hex(f);
            string decoded = decode_frame(f, rpm, temp, gear, ws, dtc, desc);

            log << time_local_now() << ","
                << fixed << setprecision(6) << ts << ",vcan0,"
                << "0x" << hex << uppercase << (f.can_id & CAN_SFF_MASK) << nouppercase << dec << ","
                << (int)f.can_dlc << ","
                << data_hex << ","
                << node << ","
                << decoded << "\n";
            log.flush();
        }
        this_thread::sleep_for(chrono::milliseconds(5));
    }

    close(s);
    return 0;
}
