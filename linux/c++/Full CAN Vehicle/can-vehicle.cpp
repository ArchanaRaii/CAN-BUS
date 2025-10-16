#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <csignal>
#include <fcntl.h>
#include <unordered_map>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
using namespace std;

atomic<bool> running(true);
atomic<bool> engineDTC(false), transDTC(false), absDTC(false);
atomic<bool> lastEngineFault(false), lastTransFault(false), lastABSFault(false);

int open_can_socket_nonblocking(const string &ifname) {
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) return -1;

    struct ifreq ifr {};
    strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { close(s); return -1; }

    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(s); return -1; }

    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    return s;
}

string time_local_now() {
    using namespace chrono;
    auto now = system_clock::now();
    auto tt = system_clock::to_time_t(now);
    auto us = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;
    tm local_tm {}; localtime_r(&tt, &local_tm);
    stringstream ss;
    ss << put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "." 
       << setw(6) << setfill('0') << us.count();
    return ss.str();
}

string data_to_hex(const struct can_frame &f) {
    stringstream ss;
    ss << hex << setfill('0');
    for (int i = 0; i < f.can_dlc; ++i)
        ss << setw(2) << (int)f.data[i];
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
        default: return "Unknown";
    }
}

void engine_ecu(const string &ifname) {
    int s = open_can_socket_nonblocking(ifname);
    if (s < 0) return;
    struct can_frame f {}; f.can_id = 0x100; f.can_dlc = 6;
    int counter = 0;

    while (running) {
        int rpm = 800 + rand() % 1000;
        int temp = 70 + rand() % 50;
        int torque = rand() % 255;

        f.data[0] = rpm & 0xFF; f.data[1] = (rpm >> 8) & 0xFF;
        f.data[2] = torque; f.data[3] = temp;
        f.data[4] = 0; f.data[5] = 0;
        write(s, &f, sizeof(f));

        if (++counter % 40 == 0) engineDTC = !engineDTC;
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    close(s);
}

void transmission_ecu(const string &ifname) {
    int s = open_can_socket_nonblocking(ifname);
    if (s < 0) return;
    struct can_frame f {}; f.can_id = 0x120; f.can_dlc = 4;

    while (running) {
        f.data[0] = rand() % 6;
        f.data[1] = rand() % 255;
        f.data[2] = rand() % 100;
        f.data[3] = 0;
        write(s, &f, sizeof(f));

        if ((rand() % 1000) < 3) transDTC = true;
        if ((rand() % 1000) < 5) transDTC = false;
        this_thread::sleep_for(chrono::milliseconds(120));
    }
    close(s);
}

void abs_ecu(const string &ifname) {
    int s = open_can_socket_nonblocking(ifname);
    if (s < 0) return;
    struct can_frame f {}; f.can_id = 0x200; f.can_dlc = 8;

    while (running) {
        for (int i = 0; i < 4; i++) {
            int ws = 30 + rand() % 220;
            f.data[i * 2] = ws & 0xFF;
            f.data[i * 2 + 1] = (ws >> 8) & 0xFF;
        }
        write(s, &f, sizeof(f));

        if (rand() % 2000 < 3) absDTC = true;
        if (rand() % 2000 < 5) absDTC = false;
        this_thread::sleep_for(chrono::milliseconds(150));
    }
    close(s);
}

void diag_responder(const string &ifname) {
    int s = open_can_socket_nonblocking(ifname);
    if (s < 0) return;
    struct can_frame req {}, resp {};

    while (running) {
        ssize_t n = read(s, &req, sizeof(req));
        if (n > 0 && req.can_dlc >= 2) {
            uint32_t id = req.can_id & CAN_SFF_MASK;
            if (id >= 0x7E0 && id <= 0x7E2) {
                resp.can_id = id + 8;
                uint8_t svc = req.data[0], sub = req.data[1];

                if (svc == 0x19) { // Read DTC
                    resp.can_dlc = 5; resp.data[0] = 0x59; resp.data[1] = sub;
                    if (id == 0x7E0 && engineDTC) { resp.data[2]=0x02; resp.data[3]=0x17; resp.data[4]=0x00; lastEngineFault = true; }
                    else if (id == 0x7E1 && transDTC) { resp.data[2]=0x07; resp.data[3]=0x00; resp.data[4]=0x00; lastTransFault = true; }
                    else if (id == 0x7E2 && absDTC) { resp.data[2]=0xC1; resp.data[3]=0x23; resp.data[4]=0x04; lastABSFault = true; }
                    else resp.data[2] = resp.data[3] = resp.data[4] = 0;
                }
                else if (svc == 0x14) { // Clear DTC
                    resp.can_dlc = 2; resp.data[0] = 0x54; resp.data[1] = sub;
                    if (id == 0x7E0) engineDTC = false;
                    if (id == 0x7E1) transDTC = false;
                    if (id == 0x7E2) absDTC = false;
                }
                write(s, &resp, sizeof(resp));
            }
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    close(s);
}

void diag_tester(const string &ifname) {
    int s = open_can_socket_nonblocking(ifname);
    if (s < 0) return;
    struct can_frame req {};

    while (running) {
        for (uint32_t id = 0x7E0; id <= 0x7E2; id++) {
            req.can_id = id; req.can_dlc = 2;
            req.data[0] = 0x19; req.data[1] = 0x02;
            write(s, &req, sizeof(req));
            this_thread::sleep_for(chrono::milliseconds(200));

            if ((id == 0x7E0 && lastEngineFault) ||
                (id == 0x7E1 && lastTransFault) ||
                (id == 0x7E2 && lastABSFault)) {
                req.data[0] = 0x14; req.data[1] = 0xFF;
                write(s, &req, sizeof(req));
                this_thread::sleep_for(chrono::milliseconds(200));
                if (id == 0x7E0) lastEngineFault = false;
                if (id == 0x7E1) lastTransFault = false;
                if (id == 0x7E2) lastABSFault = false;
            }
            this_thread::sleep_for(chrono::seconds(2));
        }
    }
    close(s);
}

string decode_dtc(uint8_t a, uint8_t b) {
    if (a == 0 && b == 0) return "None";
    char letter;
    switch (a >> 6) {
        case 0b00: letter = 'P'; break;
        case 0b01: letter = 'C'; break;
        case 0b10: letter = 'B'; break;
        default:   letter = 'U'; break;
    }
    int code_num = ((a & 0x3F) << 8) | b;
    char formatted[10];
    snprintf(formatted, sizeof(formatted), "%c%04X", letter, code_num);
    return formatted;
}

void receiver_dashboard(const string &ifname) {
    int s = open_can_socket_nonblocking(ifname);
    if (s < 0) return;
    struct can_frame f {};

    ofstream log("vehicle_decoded_log.csv");
    log << "time_local,ts_mono,bus,can_id,dlc,data_hex,node,decoded_values\n";

    auto start = chrono::steady_clock::now();
    int rpm=0, temp=0, gear=0, ws=0;
    string dtc="None", last_state="None";

    unordered_map<string, string> dtc_description = {
        {"P0217", "Engine Overheat"},
        {"P0700", "Transmission System Fault"},
        {"C1234", "ABS Wheel Speed Sensor Fault"}
    };

    while (running) {
        ssize_t n = read(s, &f, sizeof(f));
        if (n > 0) {
            auto now = chrono::steady_clock::now();
            double ts = chrono::duration<double>(now - start).count();
            string hex = data_to_hex(f);
            uint32_t id = f.can_id & CAN_SFF_MASK;
            string node = node_name(id);

            if (id == 0x100) { rpm = f.data[0] | (f.data[1]<<8); temp = f.data[3]; }
            else if (id == 0x120) { gear = f.data[0]; }
            else if (id == 0x200) { ws = f.data[0] | (f.data[1]<<8); }
            else if (id >= 0x7E8 && id <= 0x7EA) {
                if (f.data[0] == 0x59 && f.data[2] != 0)
                    dtc = "Active:" + decode_dtc(f.data[2], f.data[3]);
                else if (f.data[0] == 0x54)
                    dtc = "Cleared";
                else dtc = "None";
            }

            stringstream decoded;
            decoded << "RPM=" << rpm << ",Temp=" << temp
                    << "C,Gear=" << gear << ",WS=" << ws << ",DTC=" << dtc;

            log << time_local_now() << "," << fixed << setprecision(6) << ts
                << ",vcan0,0x" << hex << "," << (int)f.can_dlc << ","
                << hex << "," << node << "," << decoded.str() << "\n";
            log.flush();

            // Readable terminal output
            string dtc_code = "";
            if (dtc.find("Active") != string::npos) dtc_code = dtc.substr(dtc.find(":") + 1);
            string desc = dtc_description.count(dtc_code) ? dtc_description[dtc_code] : "Unknown Fault";

            if (dtc.find("Active") != string::npos && last_state != "Active") {
                cout << "\033[31m[FAULT]\033[0m " << time_local_now()
                     << " | " << node << " | " << desc << " (" << dtc_code << ")\n";
                last_state = "Active";
            } 
            else if (dtc == "Cleared" && last_state == "Active") {
                cout << "\033[32m[RECOVERY]\033[0m " << time_local_now()
                     << " | " << node << " | Fault cleared successfully\n";
                last_state = "Cleared";
            }
        }
        this_thread::sleep_for(chrono::milliseconds(50));
    }
    log.close();
    close(s);
}

void sigint_handler(int){ running = false; }

int main() {
    signal(SIGINT, sigint_handler);
    string iface = "vcan0";

    thread t1(engine_ecu, iface);
    thread t2(transmission_ecu, iface);
    thread t3(abs_ecu, iface);
    thread t4(diag_responder, iface);
    thread t5(diag_tester, iface);
    thread t6(receiver_dashboard, iface);

    cout << "Vehicle CAN Simulation running on " << iface << endl;
    cout << "Press Ctrl+C to stop.\n";

    t1.join(); t2.join(); t3.join(); t4.join(); t5.join(); t6.join();
    cout << "Simulation stopped." << endl;
    return 0;
}
