#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <set>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <cstring>
using namespace std;

// Accepted CAN IDs
set<unsigned int> acceptedIDs = {0x100};

ofstream logFile;

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

// Receiver node with filter
void receiverThread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    cout << "[Receiver] Listening on " << ifname << "...\n";

    while(true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if(nbytes < 0) {
            perror("Read");
            break;
        }

        bool isExtended = frame.can_id & CAN_EFF_FLAG;
        unsigned int id = isExtended ? (frame.can_id & CAN_EFF_MASK)
                                     : (frame.can_id & CAN_SFF_MASK);

        bool accepted = acceptedIDs.find(id) != acceptedIDs.end();

        // Only print accepted messages in console
        if(accepted) {
            cout << "[Receiver] Received CAN ID=0x" << hex << uppercase << id
                 << " DLC=" << dec << (int)frame.can_dlc << " Data=[";
            for(int i=0;i<frame.can_dlc;i++){
                cout << hex << setw(2) << setfill('0') << (int)frame.data[i];
                if(i<frame.can_dlc-1) cout << " ";
            }
            cout << "]" << endl;
        }

        // Log all messages to CSV (processed vs dropped)
        logFile << hex << uppercase << id << "," 
                << (accepted ? "Processed" : "Dropped") << ","
                << dec << (int)frame.can_dlc << ",";
        for(int i=0;i<frame.can_dlc;i++){
            logFile << hex << setw(2) << setfill('0') << (int)frame.data[i];
            if(i<frame.can_dlc-1) logFile << " ";
        }
        logFile << endl;
        logFile.flush();
    }

    close(s);
}

// Sender node 
void senderThread(const char *ifname, unsigned int can_id) {
    int s = setupCAN(ifname);
    can_frame frame{};
    srand(time(0) + can_id);

    while(true) {
        frame.can_id = can_id;
        frame.can_dlc = 8;
        for(int i=0;i<8;i++) frame.data[i] = rand() & 0xFF;

        if(write(s, &frame, sizeof(frame)) != sizeof(frame))
            perror("Write");

        this_thread::sleep_for(chrono::seconds(2));
    }

    close(s);
}

int main() {
    const char *ifname = "vcan0";
    logFile.open("filtered_can_log.csv");
    logFile << "CAN_ID,Status,DLC,Data\n";

    thread sender1(senderThread, ifname, 0x100); // Accepted
    thread sender2(senderThread, ifname, 0x300); // Dropped
    thread receiver(receiverThread, ifname);

    sender1.join();
    sender2.join();
    receiver.join();

    logFile.close();
    return 0;
}
