#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <cstdlib>
#include <ctime>

using namespace std;

const int FIXED_DLC = 8;

// Random CAN ID
unsigned int randomCANID(bool extended) {
    if (extended) return (rand() & 0x1FFFFFFF); // 29-bit
    else return (rand() & 0x7FF);               // 11-bit
}

// Random data
void randomData(can_frame &frame) {
    frame.can_dlc = FIXED_DLC;
    for (int i = 0; i < FIXED_DLC; i++)
        frame.data[i] = rand() & 0xFF;
}

// Sender thread
void senderThread(const char *ifname) {
    int s;
    sockaddr_can addr;
    ifreq ifr;

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

    can_frame frame;

    while (true) {
        // Random standard frame
        bool extended = false;
        frame.can_id = randomCANID(extended);
        randomData(frame);
        write(s, &frame, sizeof(frame));

        // Random extended frame
        extended = true;
        frame.can_id = randomCANID(extended) | CAN_EFF_FLAG;
        randomData(frame);
        write(s, &frame, sizeof(frame));
    }

    close(s);
}

// Receiver thread with CSV
void receiverThread(const char *ifname) {
    int s;
    sockaddr_can addr;
    ifreq ifr;

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

    can_frame frame;
    cout << "[Receiver] Listening on " << ifname << " ..." << endl;

    ofstream csv("can_log.csv");
    csv << "Timestamp,CAN_ID,Type,DLC,Data\n";

    while (true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("Receiver Read");
            break;
        }

        bool isExtended = frame.can_id & CAN_EFF_FLAG;
        unsigned int id = isExtended ? (frame.can_id & CAN_EFF_MASK)
                                     : (frame.can_id & CAN_SFF_MASK);

        // Timestamp
        auto now = chrono::system_clock::now();
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
        time_t t = chrono::system_clock::to_time_t(now);
        tm tm = *localtime(&t);
        ostringstream timestamp;
        timestamp << put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << setw(3) << setfill('0') << ms.count();

        cout << "[" << timestamp.str() << "] ID=0x" << hex << uppercase << id
             << (isExtended ? " (Extended)" : " (Standard)") << " DLC=" << dec << (int)frame.can_dlc << " Data=[";
        for (int i = 0; i < frame.can_dlc; i++) {
            cout << hex << uppercase << setw(2) << setfill('0') << (int)frame.data[i];
            if (i < frame.can_dlc - 1) cout << " ";
        }
        cout << "]" << endl;

        // CSV
        csv << timestamp.str() << ",0x" << hex << uppercase << id << ","
            << (isExtended ? "Extended" : "Standard") << ","
            << dec << (int)frame.can_dlc << ",";
        for (int i = 0; i < frame.can_dlc; i++) {
            csv << hex << uppercase << setw(2) << setfill('0') << (int)frame.data[i];
            if (i < frame.can_dlc - 1) csv << " ";
        }
        csv << "\n";
        csv.flush();
    }

    csv.close();
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
