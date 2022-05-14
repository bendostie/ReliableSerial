/*
* Author: Benjamin Dostie modified from work in Arduino playground and updated by Manash Kumar Mandal
* LICENSE: MIT
*/


#include <windows.h>
#include <iostream>
#include <string>


class Serial
{
private:
    HANDLE handler;
    bool connected;
    COMSTAT status;
    DWORD errors;

    //next packet number to send
    unsigned short sendSeq;
    //last received ack on data sending side
    unsigned short lastAckedPacket;
    //last nack received on data sending side
    unsigned int resendPacket;
    //next packet expected on recieve side
    unsigned short readSeq;

    //buffer for control packets
    void* controlSendBuffer;


    //length of data in packet
    unsigned int packetDataLength;
    //length of total packet
    unsigned int packetLength;
    //number of packets buffer can store
    unsigned int bufferLength;
    //buffer for storing data for application
    char* receivedDataBuffer;
    //offset of last new byte to enter the buffer
    int lastReceivedByte;
    //offset of buffer to next byte to read to application
    int readOffset;

public:
    bool fillBuffer();
    unsigned short crc(const char* message, unsigned int length);
    bool ack(unsigned short seq);
    bool nack(unsigned short seq);
    explicit Serial(const char *portName);
    ~Serial();
    bool readPacket();
    bool write(const void *buffer, unsigned int bufSize);
    char read();
    int send(const void* message, unsigned int messageLength);
    bool isConnected();
    bool print(std::string message);
    void closeSerial();
    bool available();
};