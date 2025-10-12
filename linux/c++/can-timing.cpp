#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

using namespace std;

// Setup CAN socket
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

// Random 8-byte data
void randomData(can_frame &frame) {
    for (int i = 0; i < 8; i++)
        frame.data[i] = rand() & 0xFF;
    frame.can_dlc = 8;
}

// Sensor1: periodic sender every 1s
void sensor1Thread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    frame.can_id = 0x101; // Sensor1 ID
    srand(time(0)+1);

    while (true) {
        randomData(frame);
        if (write(s, &frame, sizeof(frame)) != sizeof(frame)) perror("Write");
        this_thread::sleep_for(chrono::seconds(1));
    }
    close(s);
}

// Sensor2: send on value change
void sensor2Thread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    frame.can_id = 0x102; // Sensor2 ID
    srand(time(0)+2);

    uint8_t lastValue = 0;
    while (true) {
        uint8_t newValue = rand() % 256; // single-byte sensor
        if (newValue != lastValue) {
            frame.can_dlc = 1;
            frame.data[0] = newValue;
            if (write(s, &frame, sizeof(frame)) != sizeof(frame)) perror("Write");
            lastValue = newValue;
        }
        this_thread::sleep_for(chrono::milliseconds(100)); // check frequently
    }
    close(s);
}

// Dashboard receiver + logger
void dashboardThread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    cout << "[Dashboard] Listening on " << ifname << "...\n";

    ofstream log("dashboard_log.csv");
    log << "Timestamp,CAN_ID,Data\n";

    while (true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) { perror("Read"); break; }

        auto now = chrono::system_clock::now();
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
        time_t t = chrono::system_clock::to_time_t(now);
        tm tm = *localtime(&t);
        ostringstream timestamp;
        timestamp << put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << setw(3) << setfill('0') << ms.count();

        unsigned int id = frame.can_id & CAN_SFF_MASK;

        cout << "[" << timestamp.str() << "] ID=0x" << hex << id << " Data=[";
        for (int i=0; i<frame.can_dlc; i++) {
            cout << setw(2) << setfill('0') << hex << (int)frame.data[i];
            if(i<frame.can_dlc-1) cout << " ";
        }
        cout << "]\n";

        log << timestamp.str() << ",0x" << hex << id << ",";
        for(int i=0;i<frame.can_dlc;i++) {
            log << setw(2) << setfill('0') << hex << (int)frame.data[i];
            if(i<frame.can_dlc-1) log << " ";
        }
        log << "\n";
        log.flush();
    }
    close(s);
}

int main() {
    const char *ifname = "vcan0";

    thread s1(sensor1Thread, ifname);
    thread s2(sensor2Thread, ifname);
    thread dash(dashboardThread, ifname);

    s1.join();
    s2.join();
    dash.join();

    return 0;
}
