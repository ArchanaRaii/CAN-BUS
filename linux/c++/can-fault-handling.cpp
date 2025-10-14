#include <iostream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;
mutex log_mutex;

string now() {
    time_t t = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return string(buf);
}

struct NodeStatus {
    int tx_errors = 0;
    int rx_errors = 0;
    string state = "OK";
    chrono::steady_clock::time_point bus_off_start;
};

// Logging functions
void logFiltered(canid_t id, const string& message, NodeStatus &node) {
    lock_guard<mutex> lock(log_mutex);
    ofstream file("filtered_messages.csv", ios::app);
    file << now() << ",0x" << hex << id << dec << "," << message
         << "," << node.tx_errors << "," << node.rx_errors << "," << node.state << "\n";
}

void logDropped(can_frame &frame, NodeStatus &node) {
    lock_guard<mutex> lock(log_mutex);
    ofstream file("dropped_messages.csv", ios::app);
    stringstream ss;
    for (int i = 0; i < frame.can_dlc; i++)
        ss << setfill('0') << setw(2) << hex << (int)frame.data[i];

    file << now() << ",0x" << hex << frame.can_id << dec
         << "," << ss.str()
         << "," << node.tx_errors
         << "," << node.rx_errors
         << "," << node.state << "\n";
}

void logDTC(const string& dtc_code, NodeStatus &node, const string& desc) {
    lock_guard<mutex> lock(log_mutex);
    ofstream file("diagnostic_events.csv", ios::app);
    file << now() << "," << dtc_code
         << "," << node.tx_errors
         << "," << node.rx_errors
         << "," << node.state
         << "," << desc << "\n";
}

// Update node state based on TEC/REC
void updateState(NodeStatus &node) {
    if (node.tx_errors >= 255) {
        if (node.state != "BUS_OFF") node.bus_off_start = chrono::steady_clock::now();
        node.state = "BUS_OFF";
    } else if (node.tx_errors >= 127 || node.rx_errors >= 127) {
        node.state = "WARNING";
    } else {
        node.state = "OK";
    }
}

// Sender node simulating temperature sensor
void senderNode(int tx_sock, sockaddr_can &addr, canid_t normalID, canid_t dtcID, NodeStatus &node, const string& nodeName) {
    while (true) {
        if (node.state == "BUS_OFF") {
            auto now_time = chrono::steady_clock::now();
            if (chrono::duration_cast<chrono::seconds>(now_time - node.bus_off_start).count() >= 2) {
                node.tx_errors = 0;
                node.rx_errors = 0;
                node.state = "OK";
                cout << "[INFO] " << nodeName << " recovered from BUS_OFF" << endl;
            } else {
                this_thread::sleep_for(chrono::milliseconds(200));
                continue;
            }
        }

        can_frame frame = {};
        int temperature = 70 + rand() % 60; 

        if (temperature < 88) { 
            frame.can_id = dtcID;
            frame.can_dlc = 3;
            frame.data[0] = 0x43;
            frame.data[1] = 0x01;
            frame.data[2] = 0x28;
            node.tx_errors += 8;
        } else if (temperature > 120) { /
            frame.can_id = dtcID;
            frame.can_dlc = 3;
            frame.data[0] = 0x43;
            frame.data[1] = 0x02;
            frame.data[2] = 0x17;
            node.tx_errors += 8;
        } else { 
            frame.can_id = normalID;
            frame.can_dlc = 2;
            frame.data[0] = temperature;
            frame.data[1] = 0;
            if (node.tx_errors > 0) node.tx_errors -= 1;
        }

        if (sendto(tx_sock, &frame, sizeof(frame), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("sendto");
            node.tx_errors += 8;
        }

        updateState(node);
        this_thread::sleep_for(chrono::milliseconds(200 + rand() % 200));
    }
}

// DTC decoding
struct DTCMessage {
    string dtc_code;
    string desc;
    int temp_trigger;
};

DTCMessage decodeDTC(can_frame &frame) {
    if (frame.data[0] == 0x43) {
        if (frame.data[1] == 0x01 && frame.data[2] == 0x28)
            return {"P0128", "Low Engine Temp", 87};
        else if (frame.data[1] == 0x02 && frame.data[2] == 0x17)
            return {"P0217", "High Engine Temp", 121};
    }
    return {"", "", 0};
}

// Normal temperature decoding
string decodeNormal(can_frame &frame) {
    int temp = frame.data[0];
    return "Temperature: " + to_string(temp) + "°C";
}

// Receiver node
void receiver(int rx_sock, NodeStatus &node) {
    can_frame frame;

    while (true) {
        int nbytes = read(rx_sock, &frame, sizeof(frame));
        if (nbytes < 0) continue;

        // Simulate 5% bus errors
        bool bus_error = (rand() % 100) < 5;
        if (bus_error) node.rx_errors += 1;
        else if (node.rx_errors > 0) node.rx_errors -= 1;

        updateState(node);

        string message;
        bool isDTC = false;

        // Decode DTC
        DTCMessage dtc_msg = decodeDTC(frame);
        if (!dtc_msg.dtc_code.empty()) {
            message = "Temperature: " + to_string(dtc_msg.temp_trigger) + "°C ";
            isDTC = true;
            logDTC(dtc_msg.dtc_code, node, dtc_msg.desc); // Log with description
        }
        // Decode normal temperature
        else if ((frame.can_id & CAN_SFF_MASK) == 0x100 || (frame.can_id & CAN_SFF_MASK) == 0x7e8) {
            message = decodeNormal(frame);
        } else {
            logDropped(frame, node); // log dropped frames with CAN ID
            continue;
        }

        cout << "[PROCESSED] " << now() 
             << " ID=0x" << hex << frame.can_id << dec
             << " " << message
             << " DTC=" << (dtc_msg.dtc_code.empty() ? "None" : dtc_msg.dtc_code)
             << " TEC=" << node.tx_errors
             << " REC=" << node.rx_errors
             << " State=" << node.state << endl;

        logFiltered(frame.can_id, message, node);
    }
}

int main() {
    srand(time(nullptr));

    ofstream filtered("filtered_messages.csv"); 
    filtered << "timestamp,CAN_ID,Message,TEC,REC,State\n"; 
    filtered.close();

    ofstream dropped("dropped_messages.csv"); 
    dropped << "timestamp,CAN_ID,Data,TEC,REC,State\n"; 
    dropped.close();

    ofstream dtc("diagnostic_events.csv"); 
    dtc << "timestamp,DTC_Code,TEC,REC,State,Description\n"; 
    dtc.close();

    NodeStatus nodeA, nodeB;

    int rx_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (rx_sock < 0) { perror("RX socket"); return 1; }
    struct ifreq ifr;
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(rx_sock, SIOCGIFINDEX, &ifr);
    struct sockaddr_can addr = {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(rx_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind RX"); return 1; }

    int tx_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (tx_sock < 0) { perror("TX socket"); return 1; }
    if (bind(tx_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind TX"); return 1; }

    thread senderA(senderNode, tx_sock, ref(addr), 0x100, 0x7E8, ref(nodeA), "NodeA");
    thread senderB(senderNode, tx_sock, ref(addr), 0x200, 0x7E8, ref(nodeB), "NodeB");

    receiver(rx_sock, nodeA);

    senderA.join();
    senderB.join();
    close(rx_sock);
    close(tx_sock);
    return 0;
}
