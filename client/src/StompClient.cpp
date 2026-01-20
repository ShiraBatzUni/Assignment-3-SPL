#include <iostream>
#include <thread>
#include <sstream>
#include <vector>
#include "../include/StompProtocol.h"

using namespace std;

int main(int argc, char *argv[]) {
    StompProtocol protocol;
    thread socketThread;

    while (true) {
        if (protocol.shouldTerminateClient()) break;

         if (protocol.isWaitingForReceipt()) {
             std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
             continue;
           }

        string line;
        if (!getline(cin, line)) break; 
        
        stringstream ss(line);
        string command;
        ss >> command;

        if (command == "login") {
            string hostPort, user, pass;
            ss >> hostPort >> user >> pass;
            
            size_t colonPos = hostPort.find(':');
            if (colonPos != string::npos) {
                string host = hostPort.substr(0, colonPos);
                short port = (short)stoi(hostPort.substr(colonPos + 1));

                if (protocol.connect(host, port, user, pass)) {
                    // הפעלת ת'רד האזנה ברקע
                    socketThread = thread(&StompProtocol::runSocketListener, &protocol);
                    socketThread.detach(); 
                }
            }
        } 
        else if (command == "join") {
            string gameName; ss >> gameName;
            protocol.sendJoin(gameName);
        } 
        else if (command == "report") {
            string jsonFile; ss >> jsonFile;
            protocol.sendReport(jsonFile);
        } 
        else if (command == "summary") {
            string gameName, user, fileName;
            ss >> gameName >> user >> fileName;
            protocol.saveSummary(gameName, user, fileName);
        } 
        else if (command == "logout") {
            protocol.sendLogout();
        }
    }
    return 0;
}