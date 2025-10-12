#include <iostream>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

using namespace std;

const unsigned int ENGINE_TEMP_ID = 0x123;

// Sender thread — simulates engine temperature
void senderThread(const char *ifname) {
    int s;
    sockaddr_can addr;
    ifreq ifr;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Sender Socket");
        return;
    }

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        return;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind Sender");
        return;
    }

    can_frame frame;
    frame.can_id = ENGINE_TEMP_ID;
    frame.can_dlc = 2;

    cout << "[Sender] Engine Temperature Simulator running on " << ifname << " ..." << endl;

    while (true) {
        float temp = 70.0 + static_cast<float>(rand() % 400) / 10.0;
        uint16_t temp_raw = static_cast<uint16_t>(temp * 10);  // scale *10

        // Pack data (little-endian)
        frame.data[0] = temp_raw & 0xFF;
        frame.data[1] = (temp_raw >> 8) & 0xFF;

        // Send CAN frame
        if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
            perror("Write");
        }

        this_thread::sleep_for(chrono::seconds(1));  // Send every 1s
    }

    close(s);
}

// Receiver thread — reads and decodes temperature
void receiverThread(const char *ifname) {
    int s;
    sockaddr_can addr;
    ifreq ifr;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Receiver Socket");
        return;
    }

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        return;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind Receiver");
        return;
    }

    can_frame frame;
    cout << "[Receiver] Listening on " << ifname << " ..." << endl;

    while (true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("Read");
            break;
        }

        if (frame.can_id == ENGINE_TEMP_ID && frame.can_dlc == 2) {
            uint16_t temp_raw = (frame.data[1] << 8) | frame.data[0];
            float temperature = temp_raw / 10.0; // convert back


            auto now = chrono::system_clock::now();
            time_t t = chrono::system_clock::to_time_t(now);
            tm tm = *localtime(&t);
            cout << "[" << put_time(&tm, "%H:%M:%S") << "] "
                 << "Received Temp: " << fixed << setprecision(1)
                 << temperature << " °C" << endl;
        }
    }

    close(s);
}

int main() {
    srand(time(0));

    const char *ifname = "vcan0";

    thread sender(senderThread, ifname);
    thread receiver(receiverThread, ifname);

    sender.join();
    receiver.join();

    return 0;
}
