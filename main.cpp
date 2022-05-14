#include <iostream>
#include "Serial.hpp"
#include <stdio.h>
#include <string.h>

using namespace std;

const char* portName = "\\\\.\\COM4";

//Arduino Serial object
Serial *arduino;


int main(){
    arduino = new Serial(portName);
    string message = "hello there, enjoy no zero termination!";
    std::cout << message << std::endl;
    arduino->print(message);
    while(arduino->available()){
        cout << arduino->read() << endl;
    }

}

