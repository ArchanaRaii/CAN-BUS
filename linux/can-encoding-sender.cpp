#include <iostream>
#include <cstdint>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

// Little-endian 16-bit signal pack
void setSignalLE(uint8_t* data, uint16_t rawValue, uint8_t startBit) {
    for(int i=0; i<16; ++i) {
        int bitPos = startBit + i;
        int byteIndex = bitPos / 8;
        int bitIndex  = bitPos % 8;

        data[byteIndex] &= ~(1 << bitIndex);        // clear
        if(rawValue & (1 << i)) data[byteIndex] |= (1 << bitIndex); // set
    }
}
int main() {
    // Real values
    double voltage = 12.6;   
    double rpm = 3000.0;     
    double tempC = 90.0;     

    // Convert to raw
    uint16_t raw_voltage = static_cast<uint16_t>(voltage / 0.01);
    uint16_t raw_rpm     = static_cast<uint16_t>(rpm / 0.125);
    uint16_t raw_temp    = static_cast<uint16_t>((tempC - (-273)) / 0.03125);

    // Print raw decimal values
    std::cout << "Raw signal values (decimal):\n";
    std::cout << "Voltage: " << raw_voltage << "\n";
    std::cout << "RPM: " << raw_rpm << "\n";
    std::cout << "Temperature: " << raw_temp << "\n\n";

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

    // Prepare frame
    struct can_frame frame{};
    frame.can_id  = 0x18FF50E5 | CAN_EFF_FLAG; // 29-bit extended
    frame.can_dlc = 8;
    memset(frame.data, 0, 8);

    // Pack signals
    setSignalLE(frame.data, raw_voltage, 0);   // start bit 0
    setSignalLE(frame.data, raw_rpm, 24);      // start bit 24
    setSignalLE(frame.data, raw_temp, 40);     // start bit 40

    // Print encoded CAN frame bytes
    std::cout << "Encoded CAN data bytes: ";
    for(int i=0; i<frame.can_dlc; i++)
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(frame.data[i]) << " ";
    std::cout << std::dec << "\n";

    // Send 
    if(write(s, &frame, sizeof(frame)) != sizeof(frame))
        perror("Write");

    close(s);
    return 0;
}

