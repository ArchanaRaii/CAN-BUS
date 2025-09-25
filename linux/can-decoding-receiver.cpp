#include <iostream>
#include <cstdint>
#include <unistd.h>
#include <cstring>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

// Little-endian 16-bit signal unpack
uint16_t getSignalLE(uint8_t* data, uint8_t startBit) {
    uint16_t value = 0;
    for(int i=0; i<16; ++i) {
        int bitPos = startBit + i;
        int byteIndex = bitPos / 8;
        int bitIndex  = bitPos % 8;
        if(data[byteIndex] & (1 << bitIndex))
            value |= (1 << i);
    }
    return value;
}
int main() {
    // Setup SocketCAN
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(s < 0) { perror("Socket"); return 1; }

    struct ifreq ifr;
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    bind(s, (struct sockaddr*)&addr, sizeof(addr));

    struct can_frame frame{};
    int nbytes = read(s, &frame, sizeof(frame));
    if(nbytes < 0) { perror("Read"); return 1; }

    if((frame.can_id & 0x1FFFFFFF) == 0x18FF50E5) { // check ID
        double voltage = getSignalLE(frame.data, 0) * 0.01;
        double rpm     = getSignalLE(frame.data, 24) * 0.125;
        double tempC   = getSignalLE(frame.data, 40) * 0.03125 - 273;
    
        std::cout << "Decoded signals:\n";
        std::cout << "Voltage: " << voltage << " V\n";
        std::cout << "RPM: " << rpm << "\n";
        std::cout << "Temperature: " << tempC << " °C\n";
    }

    close(s);
    return 0;
}
