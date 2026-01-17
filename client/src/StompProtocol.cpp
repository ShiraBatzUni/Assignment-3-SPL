#include <stdlib.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include "../include/StompProtocol.h"

using namespace std;

int main(int argc, char *argv[]) {
    StompProtocol protocol;
    while (true) {
        // בדיקה האם צריך לסיים (למשל אחרי Logout או ניתוק מהשרת)
        if (protocol.shouldTerminateClient()) {
            break;
        }

        const short bufsize = 1024;
        char buf[bufsize];
        cin.getline(buf, bufsize);
        string line(buf);
        
        // פירוק הפקודה למילים
        stringstream ss(line);
        string segment;
        vector<string> args;
        while(getline(ss, segment, ' ')) {
            args.push_back(segment);
        }

        if (args.empty()) continue;
        string command = args[0];

        if (command == "login") {
            if (args.size() >= 4) {
                string hostPort = args[1];
                string user = args[2];
                string pass = args[3];
                
                // חילוץ Host ו-Port מהמחרוזת
                size_t colonPos = hostPort.find(':');
                if (colonPos == string::npos) {
                    cout << "Invalid host:port" << endl;
                    continue;
                }
                string host = hostPort.substr(0, colonPos);
                short port = (short)stoi(hostPort.substr(colonPos + 1));

                // ניסיון התחברות
                if (protocol.connect(host, port, user, pass)) {
                    cout << "Login successful" << endl;
                    // הפעלת ה-Thread שמאזין לשרת ברקע
                    thread socketThread(&StompProtocol::runSocketListener, &protocol);
                    socketThread.detach(); 
                }
            } else {
                cout << "Usage: login {host:port} {user} {pass}" << endl;
            }
        } 
        else if (command == "join") {
            if (args.size() > 1) protocol.sendJoin(args[1]);
        }
        else if (command == "exit") {
            if (args.size() > 1) protocol.sendExit(args[1]);
        }
        else if (command == "report") {
            if (args.size() > 1) protocol.sendReport(args[1]);
        }
        else if (command == "logout") {
            protocol.sendLogout();
            // הלולאה תמשיך לרוץ עד שה-Thread השני יקבל אישור וידליק את הדגל shouldTerminate
        }
        else if (command == "summary") {
            cout << "Summary not implemented" << endl;
        }
        else {
            cout << "Unknown command" << endl;
        }
    }
    return 0;
}