#include <iostream>
#include <iomanip>
#include <cstring>
#include <unistd.h>         
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main() {
    const char *ifname = "vcan0";  

    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket");
        return 1;
    }

    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl");
        return 1;
    }

    struct sockaddr_can addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    struct can_frame frame;
    frame.can_id = 0x123;     
    frame.can_dlc = 2;        
    frame.data[0] = 0xAB;     
    frame.data[1] = 0xCD;


    int nbytes = write(s, &frame, sizeof(frame));
    if (nbytes < 0) {
        perror("write");
        return 1;
    }

    std::cout << "Sent " << nbytes << " bytes of "
                <<"Data: ";
                for (int i = 0; i < frame.can_dlc; i++) {
                    std::cout << "0x" << std::hex << std::uppercase 
                            << std::setw(2) << std::setfill('0') << (int)frame.data[i] << " ";
                }
    std::cout << std::dec << " on " << ifname
              << " with CAN ID 0x" << std::hex << frame.can_id << std::dec << "\n";


    close(s);

    return 0;
}
