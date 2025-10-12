#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

using namespace std;

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

void randomData(can_frame &frame) {
    for (int i = 0; i < 8; i++)
        frame.data[i] = rand() & 0xFF;
    frame.can_dlc = 8;
}

// Sender node
void senderThread(const char *ifname, unsigned int can_id, const string &name) {
    int s = setupCAN(ifname);
    can_frame frame{};
    frame.can_id = can_id;
    srand(time(0) + can_id); 

    while (true) {
        randomData(frame);
        if (write(s, &frame, sizeof(frame)) != sizeof(frame))
            perror("Write");
        this_thread::sleep_for(chrono::seconds(2)); 
    }
    close(s);
}

// Receiver node
void receiverThread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    cout << "[Receiver] Listening on " << ifname << "...\n";

    while (true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("Read");
            break;
        }

        bool isExtended = frame.can_id & CAN_EFF_FLAG;
        unsigned int id = isExtended ? (frame.can_id & CAN_EFF_MASK)
                                     : (frame.can_id & CAN_SFF_MASK);

        cout << "[Receiver] Received CAN ID=0x" << hex << id << " Data=[";
        for (int i = 0; i < frame.can_dlc; i++) {
            cout << setw(2) << setfill('0') << hex << (int)frame.data[i];
            if (i < frame.can_dlc - 1) cout << " ";
        }
        cout << "]" << endl;
    }
    close(s);
}

int main() {
    const char *ifname = "vcan0";

    thread senderA(senderThread, ifname, 0x100, "SenderA"); 
    thread senderB(senderThread, ifname, 0x300, "SenderB"); 
    thread receiver(receiverThread, ifname);

    senderA.join();
    senderB.join();
    receiver.join();

    return 0;
}
