#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <atomic>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <cstring>

using namespace std;

// Global atomic to track total bits per second
atomic<uint64_t> totalBits(0);
atomic<uint64_t> frameBitsInSecond(0);

// Setup CAN socket
int setupCAN(const char *ifname) {
    int s;
    sockaddr_can addr{};
    ifreq ifr{};
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) { perror("Socket"); exit(1); }
    std::strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("Bind"); exit(1); }
    return s;
}

// Random 8-byte data
void randomData(can_frame &frame) {
    for (int i = 0; i < 8; i++)
        frame.data[i] = rand() & 0xFF;
    frame.can_dlc = 8;
}

// Approx frame size in bits (11-bit standard)
int frameSizeBits(const can_frame &frame) {
    return 47 + frame.can_dlc*8;
}

// High-frequency sender node
void highFreqSender(const char *ifname, unsigned int can_id, const string &name) {
    int s = setupCAN(ifname);
    can_frame frame{};
    frame.can_id = can_id;
    srand(time(0) + can_id);

    while(true) {
        randomData(frame);
        if(write(s,&frame,sizeof(frame)) != sizeof(frame)) perror("Write");
        uint64_t bits = frameSizeBits(frame);
        totalBits += bits;
        frameBitsInSecond += bits;

        // Do not print to avoid console flooding
        this_thread::sleep_for(chrono::milliseconds(10)); // 100 Hz sending
    }
    close(s);
}

// Dashboard receiver + logger
void dashboardThread(const char *ifname) {
    int s = setupCAN(ifname);
    can_frame frame{};
    cout << "[Dashboard] Listening on " << ifname << "...\n";

    ofstream log("stress_test_log.csv");
    log << "Timestamp,CAN_ID,DLC,PayloadBits,TotalBits,BusLoad,Data\n";

    auto start = chrono::steady_clock::now();

    while(true) {
        int nbytes = read(s,&frame,sizeof(frame));
        if(nbytes < 0) { perror("Read"); break; }

        auto now = chrono::system_clock::now();
        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch())%1000;
        time_t t = chrono::system_clock::to_time_t(now);
        tm tm = *localtime(&t);
        ostringstream timestamp;
        timestamp << put_time(&tm,"%Y-%m-%d %H:%M:%S") << "." << setw(3) << setfill('0') << ms.count();

        unsigned int id = frame.can_id & CAN_SFF_MASK;
        int payloadBits = frame.can_dlc*8;
        uint64_t totalBitsSnapshot = frameBitsInSecond.load();
        double busLoad = (totalBitsSnapshot/500000.0)*100; // assuming 500 kbps

        // Print to console
        cout << "[" << timestamp.str() << "] "
             << "ID=0x" << hex << id
             << " DLC=" << dec << (int)frame.can_dlc
             << " PayloadBits=" << payloadBits
             << " TotalBits=" << totalBitsSnapshot
             << " BusLoad=" << fixed << setprecision(2) << busLoad << "% "
             << "Data=[";
        for(int i=0;i<frame.can_dlc;i++){
            cout << setw(2) << setfill('0') << hex << (int)frame.data[i];
            if(i<frame.can_dlc-1) cout << " ";
        }
        cout << "]" << endl;

        // Log to CSV
        log << timestamp.str() << ",0x" << hex << id << ","
            << dec << (int)frame.can_dlc << "," << payloadBits << ","
            << totalBitsSnapshot << "," << fixed << setprecision(2) << busLoad << ",";
        for(int i=0;i<frame.can_dlc;i++){
            log << setw(2) << setfill('0') << hex << (int)frame.data[i];
            if(i<frame.can_dlc-1) log << " ";
        }
        log << "\n";
        log.flush();

        // Reset counter every second
        auto elapsed = chrono::steady_clock::now() - start;
        if(chrono::duration_cast<chrono::seconds>(elapsed).count() >= 1) {
            frameBitsInSecond = 0;
            start = chrono::steady_clock::now();
        }
    }
    close(s);
}

int main() {
    const char *ifname = "vcan0";

    thread senderA(highFreqSender, ifname, 0x100, "SenderA"); // Higher priority
    thread senderB(highFreqSender, ifname, 0x200, "SenderB"); // Lower priority
    thread dash(dashboardThread, ifname);

    senderA.join();
    senderB.join();
    dash.join();

    return 0;
}
