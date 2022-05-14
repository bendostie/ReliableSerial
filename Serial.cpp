/*
* Author: Benjamin Dostie modified from work in Arduino playground that was updated by Manash Kumar Mandal
* LICENSE: MIT
*/

#include "Serial.hpp"
#include <string>
#include <stdio.h>
#include <iostream>
#include <chrono>
#include <ctime>
#include <thread>


Serial::Serial(const char *portName) //todo: split into begin function
{
    this->connected = false;
    this->lastAckedPacket = 0;
    this->resendPacket = 131072;
    this->readSeq = 0;
    this->sendSeq = 0;
    this->lastReceivedByte = 0;
    this->packetDataLength = 8;
    this->packetLength = 4 + this->packetDataLength + 2;
    this->bufferLength = 20;
    this->receivedDataBuffer = (char*) calloc(this->bufferLength, this->packetDataLength);
    this->controlSendBuffer = calloc(this->packetLength, 1);
    this->readOffset = 0;
    this->handler = CreateFileA(static_cast<LPCSTR>(portName),
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
    if (this->handler == INVALID_HANDLE_VALUE){
        if (GetLastError() == ERROR_FILE_NOT_FOUND){
            std::cerr << "ERROR: Handle was not attached.Reason : " << portName << " not available\n";
        }else{
            std::cerr << "ERROR!!!\n";
        }
    }else{
        DCB dcbSerialParameters = {0};

        if (!GetCommState(this->handler, &dcbSerialParameters)){
            std::cerr << "Failed to get current serial parameters\n";
        }else{
            dcbSerialParameters.BaudRate = CBR_9600;
            dcbSerialParameters.ByteSize = 8;
            dcbSerialParameters.StopBits = ONESTOPBIT;
            dcbSerialParameters.Parity = NOPARITY;
            dcbSerialParameters.fDtrControl = DTR_CONTROL_ENABLE;

            if (!SetCommState(handler, &dcbSerialParameters)){
                std::cout << "ALERT: could not set serial port parameters\n";
            }else{
                this->connected = true;
                PurgeComm(this->handler, PURGE_RXCLEAR | PURGE_TXCLEAR);
                Sleep(ARDUINO_WAIT_TIME);
            }
        }
    }

}

Serial::~Serial(){
    free(this->controlSendBuffer);
    if (this->connected)
    {
        this->connected = false;
        CloseHandle(this->handler);
    }

}

//returns true if available bytes in buffer
//same as arduino
bool Serial::available(){
    this->readPacket();
    return this->readOffset!=this->lastReceivedByte;
}

bool Serial::readPacket(){

    char bufPtr[this->packetLength];
    unsigned short* seq = (unsigned short*) (bufPtr + 2);
    unsigned short* type =  (unsigned short*) (bufPtr);
    DWORD bytesRead{};
    //std::cout  << "in que: " << this->status.cbInQue << std::endl;
    //check if packet was read
    ClearCommError(this->handler, &this->errors, &this->status);  
    if (this->status.cbInQue < this->packetLength){
        return false;
    }

    //read packet to buffer
    
    ClearCommError(this->handler, &this->errors, &this->status);  
    if (!ReadFile(this->handler, (void*) bufPtr, this->packetLength, &bytesRead, NULL)){
        std::cout << "error" << std::endl;
        return false;
    }
    
    //check if packet was read
    if (bytesRead < this->packetLength){
        std::cout  << "error: packet size incorrect" << std::endl;
        return false;
    }
    
    bool valid = !this->crc((const char*) bufPtr, this->packetLength);
    //std::cout  << "seq: " << *seq << std::endl;
    //std::cout  << "type: " << *type << std::endl;
    //std::cout  << "valid: " << valid << std::endl;
    //std::cout  << "message: " << bufPtr + 4 << std::endl;
    //std::cout  << " " << std::endl;


    //check if ack or nack or in order data
    if(*type == 1 & valid){
        this->lastAckedPacket = *seq;
        return true;
    }else if(*type == 2 & valid){
        this->resendPacket = *seq;
        return true;
    }else if (*seq == this->readSeq & valid){
        this->readSeq++;
        this->ack(*seq);
        //copy data to buffer
        memcpy((void*) (this->receivedDataBuffer + this->packetDataLength*(*seq%this->bufferLength)), bufPtr+4, this->packetDataLength);
        this->lastReceivedByte = this->packetDataLength*(*seq%this->bufferLength) + (this->packetLength - 1);
        return true;
    }else if(valid){
        this->nack(this->readSeq);
        return true;
    }

    return false;
}


// a wrapper function for readPacket that behaves like arduino Serial.read(). 
// reads a byte of data
char Serial::read()
{
    //read any available packets
    while(this->readPacket());
    //read next char if available
    if (this->readOffset!=this->lastReceivedByte){
        char nextByte = *(this->receivedDataBuffer + this->readOffset);
        this->readOffset++;
        return nextByte;
    }else{
        return '\0';
    }

}
bool Serial::ack(unsigned short seq){
    char* packet = (char*) this->controlSendBuffer;
    short type = 1;
    unsigned short code= 0;
    memcpy(packet+this->packetLength -2, &code, 2);
    memcpy(packet, (void*) &type, 2);
    memcpy(packet+2, &seq, 2);
    //copy crc to buffer
    code = this->crc((const char*) packet, this->packetLength);
    memcpy(packet + 4 + this->packetDataLength, ((char*) &code)+1, 1);
    memcpy(packet + 4 + this->packetDataLength+1, (char*) &code, 1);
    this->write(packet, this->packetLength);
    this-write((const char*) packet, this->packetLength);
    std::cout << "ack: " << seq << std::endl;
    return true;
}
bool Serial::nack(unsigned short seq){
    char* packet = (char*) this->controlSendBuffer;
    short type = 2;
    unsigned short code= 0;
    memcpy(packet+this->packetLength -2, &code, 2);
    memcpy(packet, (void*) &type, 2);
    memcpy(packet+2, &seq, 2);
    //copy crc to buffer
    code = this->crc((const char*) packet, this->packetLength);
    memcpy(packet + 4 + this->packetDataLength, ((char*) &code)+1, 1);
    memcpy(packet + 4 + this->packetDataLength+1, (char*) &code, 1);
    this->write(packet, this->packetLength);
    this-write((const char*) packet, this->packetLength);
    std::cout << "nack: " << seq << std::endl;
    return true;
}
unsigned short Serial::crc(const char* message, unsigned int length){
    unsigned short remainder = 0;
    unsigned short poly = 8464;
    unsigned int offset = 0;
    bool doXor = false;

    while(offset < length*8){
        //check for one in x^16
        doXor = (remainder >= 32768);
        //shift a bit in from the message
        remainder = remainder << 1;
        remainder |= (((*(message + offset/8)) >> (7-(offset%8))) & 1);

        if (doXor){
            remainder^=poly;
        }
        offset++;
    }

   	return remainder;
}



// Sending provided buffer to serial port
// returns true if succeed, false if not
bool Serial::write(const void *buffer, unsigned int buf_size)
{
    DWORD bytesSend;
    
    if (!WriteFile(this->handler, buffer, buf_size, &bytesSend, 0))//todo: buf_size should be DWORD
    {
        ClearCommError(this->handler, &this->errors, &this->status);
        return false;
    }
    return true;
}

int Serial::send(const void* message, unsigned int messageLength){

    unsigned short startSeq = this->sendSeq;
    unsigned short* seq_ptr = &this->sendSeq;
    unsigned short type = 3;
    bool stuffed = false;
    if (messageLength%this->packetDataLength){
        stuffed = true;
    }
    unsigned int numPackets = messageLength/this->packetDataLength + stuffed;

    std::cout  << "packetlength: " << this->packetLength << std::endl;
    std::cout  << "messageLength: " << messageLength << std::endl;
    std::cout  << "numPackets: " << numPackets << std::endl;

    //buffer to hold packets
    void* tempPtr = calloc(numPackets, this->packetLength);

    // cast char for byte size increments
    char* bufPtr = (char*) tempPtr;
    const char* mesPtr = (const char*) message;
    
    //loop until all packets acknowledged
    while (this->lastAckedPacket < (startSeq + numPackets) - 1){
        //divide message into packets
        for (int i = 0; i < numPackets-stuffed; i++){
            //copy seq and type to buffer
            memcpy(bufPtr, (void*) &type, 2);
            memcpy(bufPtr+2, seq_ptr, 2);
            //copy data to buffer
            memcpy(bufPtr+4, mesPtr, this->packetDataLength);
            //copy crc to buffer
            unsigned short code = this->crc(bufPtr, this->packetLength);
            memcpy(bufPtr + 4 + this->packetDataLength, ((char*) &code)+1, 1);
            memcpy(bufPtr + 4 + this->packetDataLength+1, (char*) &code, 1);
            this->write(bufPtr, this->packetLength);
            this->sendSeq++;
            bufPtr += this->packetLength;
            mesPtr += this->packetDataLength;
            this->readPacket();
        }

        if (stuffed){   //todo: send control message indicating stuffed packet
            //copy seq to buffer
            memcpy(bufPtr, (void*) &type, 2);
            memcpy(bufPtr+2, seq_ptr, 2);
            //copy data to buffer
            memcpy(bufPtr+4, mesPtr, messageLength%this->packetDataLength);
            //copy crc to buffer
            unsigned short crc = this->crc(bufPtr, this->packetLength);
            unsigned short code = this->crc(bufPtr, this->packetLength);
            memcpy(bufPtr + 4 + this->packetDataLength, ((char*) &code)+1, 1);
            memcpy(bufPtr + 4 + this->packetDataLength+1, (char*) &code, 1);
            this->write(bufPtr, this->packetLength);
            this->sendSeq++;
            this->readPacket();   
        }
        
        //loop until all acked, timeout, 
        
        while(this->lastAckedPacket+1< this->sendSeq){
            
            while(this->readPacket()){
                //std::cout  << "stuck" << std::endl;
            } 
            
            //indicates unacked packet, resend started
            if (this->resendPacket < 131072){
                int offset = (resendPacket-startSeq);
                bufPtr = ((char*) tempPtr) + offset*this->packetLength;
                mesPtr = ((const char*) message) + offset*this->packetDataLength;
                numPackets -= offset;
                resendPacket = 131072;
                break;
            }
            
        }
        
    }
        

    free(tempPtr);
    return 1;
}

//print function that employs crc, segmentation, and header
//externally same as arduino library //todo: overload
bool Serial::print(std::string message){
    return this->send((void*) message.c_str(), message.length());
}

// Checking if serial port is connected
bool Serial::isConnected()
{
    if (!ClearCommError(this->handler, &this->errors, &this->status))
    {
        this->connected = false;
    }

    return this->connected;
}

void Serial::closeSerial()
{
    CloseHandle(this->handler);
}
