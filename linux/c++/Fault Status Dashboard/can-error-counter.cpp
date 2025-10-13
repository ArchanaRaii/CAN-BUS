#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <vector>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

// Node status thresholds
enum NodeStatus { OK, WARNING, FAIL };

// Node structure
struct Node {
    string name;
    unsigned int can_id;
    int TEC = 0; // Transmit Error Counter
    int REC = 0; // Receive Error Counter
    NodeStatus status = OK;
};

// Setup CAN socket (non-blocking)
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

// Update TEC/REC and node status
void updateErrorCounters(Node &node, ofstream &csv) {
    // Random errors for demonstration
    int txErr = rand() % 20; // 0-19 tx errors
    int rxErr = rand() % 10; // 0-9 rx errors

    node.TEC = min(node.TEC + txErr, 255);
    node.REC = min(node.REC + rxErr, 127);

    if(node.TEC >= 200) node.status = FAIL;
    else if(node.TEC >= 100) node.status = WARNING;
    else node.status = OK;

    // Auto-retry logic if bus-off
    if(node.TEC >= 255) {
        auto now = chrono::system_clock::now();
        time_t t = chrono::system_clock::to_time_t(now);
        tm tm = *localtime(&t);
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;

        cout << "[" << put_time(&tm,"%H:%M:%S") << "." << setw(3) << setfill('0') << ms.count() 
             << "] " << node.name << " BUS-OFF! Auto-retrying..." << endl << flush;

        // CSV entry for bus-off
        csv << put_time(&tm,"%H:%M:%S") << "." << setw(3) << setfill('0') << ms.count()
            << "," << node.name << ",255," << node.REC << ",BUS-OFF" << endl;
        csv.flush();

        this_thread::sleep_for(chrono::milliseconds(100)); // quick retry
        node.TEC = 0;
        node.REC = 0;
        node.status = OK;
    }
}

// Sender thread
void senderThread(Node &node, const char* ifname, ofstream &csv) {
    int bus = setupCAN(ifname);
    can_frame frame{};
    frame.can_id = node.can_id;
    frame.can_dlc = 8;

    while(true) {
        for(int i=0;i<8;i++) frame.data[i] = rand() & 0xFF;

        int n = write(bus, &frame, sizeof(frame));
        if(n != sizeof(frame)) node.TEC += 1;

        updateErrorCounters(node, csv);

        auto now = chrono::system_clock::now();
        time_t t = chrono::system_clock::to_time_t(now);
        tm tm = *localtime(&t);
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;

        // Print status
        cout << "[" << put_time(&tm,"%H:%M:%S") << "." << setw(3) << setfill('0') << ms.count() << "] "
             << node.name << " | TEC=" << node.TEC
             << " REC=" << node.REC
             << " Status=" << (node.status==OK?"OK":(node.status==WARNING?"Warning":"Fail"))
             << endl;

        // CSV log
        csv << put_time(&tm,"%H:%M:%S") << "." << setw(3) << setfill('0') << ms.count()
            << "," << node.name << "," << node.TEC << "," << node.REC << ","
            << (node.status==OK?"OK":(node.status==WARNING?"Warning":"Fail")) << endl;
        csv.flush();

        this_thread::sleep_for(chrono::milliseconds(500));
    }
    close(bus);
}

// Receiver thread
void receiverThread(Node &node, const char* ifname, ofstream &csv) {
    int bus = setupCAN(ifname);
    can_frame frame{};
    while(true) {
        int n = read(bus, &frame, sizeof(frame));
        if(n > 0) node.REC = min(node.REC+1,127); // increment REC on receive
        updateErrorCounters(node, csv);

        auto now = chrono::system_clock::now();
        time_t t = chrono::system_clock::to_time_t(now);
        tm tm = *localtime(&t);
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;

        // Print status
        cout << "[" << put_time(&tm,"%H:%M:%S") << "." << setw(3) << setfill('0') << ms.count() << "] "
             << node.name << " | TEC=" << node.TEC
             << " REC=" << node.REC
             << " Status=" << (node.status==OK?"OK":(node.status==WARNING?"Warning":"Fail"))
             << endl;

        // CSV log
        csv << put_time(&tm,"%H:%M:%S") << "." << setw(3) << setfill('0') << ms.count()
            << "," << node.name << "," << node.TEC << "," << node.REC << ","
            << (node.status==OK?"OK":(node.status==WARNING?"Warning":"Fail")) << endl;
        csv.flush();

        this_thread::sleep_for(chrono::milliseconds(100));
    }
    close(bus);
}

// Main
int main() {
    srand(time(0));
    const char* ifname = "vcan0";

    ofstream csv("node_status_log.csv");
    csv << "Timestamp,Node,TEC,REC,Status\n";

    Node ecu{"ECU Node", 0x100};
    Node sensor{"Sensor Node", 0x200};
    Node dashboard{"Dashboard Node", 0x300};

    thread t1(senderThread, ref(ecu), ifname, ref(csv));
    thread t2(senderThread, ref(sensor), ifname, ref(csv));
    thread t3(receiverThread, ref(dashboard), ifname, ref(csv));

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
