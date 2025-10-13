#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>

using namespace std;

atomic<int> TEC(0);
atomic<int> REC(0);

const int TEC_MAX = 255;
const int REC_MAX = 127;
const double ERROR_RATE_SEND = 0.1;    // 10% chance
const double ERROR_RATE_RECEIVE = 0.1; // 10% chance

atomic<uint64_t> totalMessages(0);
atomic<uint64_t> errorMessages(0);

ofstream logFile;

// Helper to get timestamp string
string timestamp() {
    auto now = chrono::system_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    time_t t = chrono::system_clock::to_time_t(now);
    tm tm = *localtime(&t);
    ostringstream oss;
    oss << put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << setw(3) << setfill('0') << ms.count();
    return oss.str();
}

// Sender node simulation
void senderNode(unsigned int can_id) {
    srand(time(0) + can_id);
    while(true) {
        bool sendError = (rand()%1000)/1000.0 < ERROR_RATE_SEND;
        if(sendError) {
            TEC = min(TEC.load()+8, TEC_MAX);
            errorMessages++;
        } else if(TEC>0) TEC--;

        totalMessages++;

        // Log
        logFile << timestamp() << ",Send," << can_id << "," << sendError
                << "," << TEC.load() << "," << REC.load() << "\n";
        this_thread::sleep_for(chrono::seconds(1));
    }
}

// Receiver node simulation
void receiverNode(unsigned int can_id) {
    srand(time(0) + can_id);
    while(true) {
        bool receiveError = (rand()%1000)/1000.0 < ERROR_RATE_RECEIVE;
        if(receiveError) REC = min(REC.load()+1, REC_MAX);
        else if(REC>0) REC--;

        // Log
        logFile << timestamp() << ",Receive," << can_id << "," << receiveError
                << "," << TEC.load() << "," << REC.load() << "\n";
        this_thread::sleep_for(chrono::seconds(1));
        logFile.flush();
    }
}

// Monitor thread
void monitor() {
    while(true) {
        cout << "[" << timestamp() << "] TEC=" << TEC.load()
             << " | REC=" << REC.load()
             << " | Total=" << totalMessages.load()
             << " | Errors=" << errorMessages.load() << endl;
        this_thread::sleep_for(chrono::seconds(1));
    }
}

int main() {
    logFile.open("tec_rec_log.csv");
    logFile << "Timestamp,Node,ID,Error,TEC,REC\n";

    thread sender(senderNode, 0x100);
    thread receiver(receiverNode, 0x200);
    thread monitorThread(monitor);

    sender.join();
    receiver.join();
    monitorThread.join();

    logFile.close();
    return 0;
}
