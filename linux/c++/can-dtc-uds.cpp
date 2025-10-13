#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <cstring>

using namespace std;

const unsigned int ECU_ID       = 0x7E0; 
const unsigned int DASHBOARD_ID = 0x7E8; 

const float TEMP_LOW  = 70.0;
const float TEMP_HIGH = 110.0;

const string DTC_LOW  = "P0128"; 
const string DTC_HIGH = "P0217"; 

int setupCAN(const char* ifname) {
    int s;
    sockaddr_can addr{};
    ifreq ifr{};
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        exit(1);
    }

    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);

    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        exit(1);
    }

    return s;
}

string getTimestamp() {
    auto now = chrono::system_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    time_t t = chrono::system_clock::to_time_t(now);
    tm tm = *localtime(&t);
    ostringstream oss;
    oss << put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << setw(3) << setfill('0') << ms.count();
    return oss.str();
}

float simulateTemperature() {
    return 65.0f + static_cast<float>(rand() % 5600) / 100.0f; 
}

string generateDTC(float temp) {
    if(temp < TEMP_LOW) return DTC_LOW;
    if(temp > TEMP_HIGH) return DTC_HIGH;
    return "";
}

// ECU node
void ecuNode(const char* ifname) {
    int bus = setupCAN(ifname);
    srand(time(0));

    while(true) {
        float temp = simulateTemperature();
        string dtc = generateDTC(temp);

        can_frame frame{};
        frame.can_id = ECU_ID;
        frame.can_dlc = 8;

        // Pack temperature (2 bytes) 
        uint16_t tempInt = static_cast<uint16_t>(temp * 10);
        frame.data[0] = tempInt >> 8;
        frame.data[1] = tempInt & 0xFF;

        // Pack full DTC string (up to 6 bytes remaining)
        memset(frame.data + 2, 0, 6); // clear
        for(size_t i = 0; i < dtc.size() && i < 6; ++i)
            frame.data[2+i] = dtc[i];

        if(write(bus, &frame, sizeof(frame)) != sizeof(frame))
            perror("ECU write");

        this_thread::sleep_for(chrono::milliseconds(500));
    }

    close(bus);
}

// Dashboard node
void dashboardNode(const char* ifname) {
    int bus = setupCAN(ifname);
    ofstream csv("uds_dtc_log.csv");
    csv << "Timestamp,Temperature,DTC\n";

    while(true) {
        can_frame frame{};
        int nbytes = read(bus, &frame, sizeof(frame));
        if(nbytes > 0 && frame.can_id == ECU_ID) {
            uint16_t tempInt = (frame.data[0] << 8) | frame.data[1];
            float temp = tempInt / 10.0f;

            string dtc = "";
            for(int i=2;i<8;i++)
                if(frame.data[i] != 0) dtc += frame.data[i];

            string ts = getTimestamp();
            cout << "[" << ts << "] Temp=" << fixed << setprecision(1) << temp 
                 << "°C DTC=" << (dtc.empty()?"None":dtc) << endl;

            csv << ts << "," << temp << "," << (dtc.empty()?"None":dtc) << "\n";
            csv.flush();
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    csv.close();
    close(bus);
}

int main() {
    const char* ifname = "vcan0";
    srand(time(0));

    thread ecu(ecuNode, ifname);
    thread dashboard(dashboardNode, ifname);

    ecu.join();
    dashboard.join();

    return 0;
}
