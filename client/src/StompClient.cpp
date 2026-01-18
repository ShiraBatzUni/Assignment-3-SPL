#include <iostream>
#include <thread>
#include <sstream>
#include "../include/StompProtocol.h"

#include "../include/StompProtocol.h"

int main(int argc, char *argv[]) {
    
    StompProtocol protocol;
    
    while (true) {
        if (protocol.shouldTerminateClient()) break;

        std::string line;
        if (!std::getline(std::cin, line)) break; // קריאה פקודה מהמקלדת [cite: 295]
        
        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command == "login") {
            // לוגיקת התחברות לסוקט ופיצול לת'רד האזנה [cite: 312, 291]
            // std::thread socketThread(&StompProtocol::runSocketListener, &protocol);
            // socketThread.detach(); 
        } else {
            // תרגום ושליחת שאר הפקודות (join, exit, report, logout) [cite: 307]
        }
    }
    return 0;
}