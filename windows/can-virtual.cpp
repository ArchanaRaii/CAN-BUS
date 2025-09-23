#include<iostream>
#include<vector>
#include <algorithm>
#include<iomanip>

using namespace std;

class CANBus{
public:
    uint32_t arbitration_id; //store both standard 11 and extended 29 bits
    bool is_remote;
    bool is_extended;//True for extended, False for standard
    uint8_t dlc; // 0 to 255 so upto 64 byte
    vector<uint8_t> data;

    CANBus(uint32_t id, bool extended, bool remote, uint8_t length, const vector<uint8_t>& payload)
        : arbitration_id(id), is_extended(extended), is_remote(remote), dlc(length), data(payload) {}

    void print(){
        cout << "CAN Frame: ID 0x" << hex << arbitration_id
             << (is_extended ? " (Extended)" : " (Standard)")
             << (is_remote ? " RTR" : "")
             << " DLC " << dec << (int)dlc
             << " Data ";
        for (size_t i = 0; i < data.size(); ++i) {
            cout << "0x" << hex << (int)data[i];
            if (i < data.size() - 1)
                cout << " ";
        }
        cout << "" <<endl;

    }
};

bool comp(const CANBus &a, const CANBus &b) {
    return a.arbitration_id < b.arbitration_id; //lower ID first
}

int main(){

    vector<CANBus> nodes;
    nodes.push_back(CANBus(0x1234, false, false, 8, {1,2,3,4,5,6,7,0}));// Standard
    nodes.push_back(CANBus(0x1ABCDE, true, false, 8, {0,16,17,18,19,20,21,22}));// Extended
    nodes.push_back(CANBus(0x1000, false, true, 0, {}));// RTR


    sort(nodes.begin(), nodes.end(), comp);

    cout << "Frames transmitted in arbitration order: " << endl;
    for (size_t i = 0; i < nodes.size(); ++i) {
        nodes[i].print();
    }



    return 0;
}
