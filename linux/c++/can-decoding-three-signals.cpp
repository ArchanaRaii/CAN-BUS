#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>         
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

using namespace std;

void encodeSignals(struct can_frame &frame, float temperature, float voltage, int rpm) {
    // Frame ID for Engine Status
    frame.can_id = 0x123;
    frame.can_dlc = 8;

    // Encoding scheme:
    // Byte0-1: temperature * 100 (int16)
    // Byte2-3: voltage * 100 (int16)
    // Byte4-7: rpm (int32)

    int16_t temp_enc = static_cast<int16_t>(temperature * 100);
    int16_t volt_enc = static_cast<int16_t>(voltage * 100);
    int32_t rpm_enc  = static_cast<int32_t>(rpm);

    frame.data[0] = temp_enc & 0xFF;
    frame.data[1] = (temp_enc >> 8) & 0xFF;
    frame.data[2] = volt_enc & 0xFF;
    frame.data[3] = (volt_enc >> 8) & 0xFF;
    frame.data[4] = rpm_enc & 0xFF;
    frame.data[5] = (rpm_enc >> 8) & 0xFF;
    frame.data[6] = (rpm_enc >> 16) & 0xFF;
    frame.data[7] = (rpm_enc >> 24) & 0xFF;
}


void decodeSignals(const struct can_frame &frame) {
    if (frame.can_id != 0x123) return;

    int16_t temp_enc = frame.data[0] | (frame.data[1] << 8);
    int16_t volt_enc = frame.data[2] | (frame.data[3] << 8);
    int32_t rpm_enc  = frame.data[4] | (frame.data[5] << 8) | (frame.data[6] << 16) | (frame.data[7] << 24);

    float temperature = temp_enc / 100.0;
    float voltage = volt_enc / 100.0;
    int rpm = rpm_enc;

    cout << fixed << setprecision(2);
    cout << "[Receiver] Temperature: " << setw(6) << temperature << " °C "
         << "Voltage: " << setw(5) << voltage << " V "
         << "RPM: " << setw(5) << rpm << endl;
}

//Sender Thread
void senderThread(const char *ifname) {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        return;
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        close(s);
        return;
    }

    srand(time(0));

    while (true) {
        float temperature = 60 + (rand() % 4000) / 100.0;  // 60–100°C
        float voltage = 11.0 + (rand() % 300) / 100.0;     // 11–14V
        int rpm = 700 + rand() % 5000;                     // 700–5700 rpm

        encodeSignals(frame, temperature, voltage, rpm);

        if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
            perror("Write");
        }

        this_thread::sleep_for(chrono::seconds(1));
    }

    close(s);
}

// Receiver Thread 
void receiverThread(const char *ifname) {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        return;
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        close(s);
        return;
    }

    cout << "Listening on vcan0...\n";

    while (true) {
        int nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("Read");
            break;
        } else if (nbytes < sizeof(struct can_frame)) {
            cerr << "Incomplete CAN frame\n";
            continue;
        }

        decodeSignals(frame);
    }

    close(s);
}


int main() {
    thread sender(senderThread, "vcan0");
    thread receiver(receiverThread, "vcan0");

    sender.join();
    receiver.join();

    return 0;
}
