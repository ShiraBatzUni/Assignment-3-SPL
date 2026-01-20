#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <fstream>
#include "../include/StompProtocol.h"

int main(int argc, char *argv[]) {
    StompProtocol protocol;

    // Thread שמאזין לשרת ברקע
    std::thread socketThread([&protocol]() {
        while (!protocol.shouldTerminateClient()) {
            if (protocol.isConnectedToSocket()) {
                protocol.runSocketListener();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    while (!protocol.shouldTerminateClient()) {
        std::string line;
        if (!std::getline(std::cin, line) || line.empty()) continue;

        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command == "login") {
            if (protocol.isConnectedToSocket()) {
                std::cout << "The client is already logged in, log out before logging in again" << std::endl;
                continue;
            }
            std::string hostPort, user, pass;
            if (!(ss >> hostPort >> user >> pass)) {
                std::cout << "Usage: login {host:port} {user} {password}" << std::endl;
                continue;
            }
            size_t colon = hostPort.find(':');
            if (colon == std::string::npos) {
                std::cout << "Invalid host:port format" << std::endl;
                continue;
            }
            std::string host = hostPort.substr(0, colon);
            short port = std::stoi(hostPort.substr(colon + 1));
            protocol.connect(host, port, user, pass);
        }
        else if (command == "summary") {
            std::string game, user, file;
            if (!(ss >> game >> user >> file)) {
                std::cout << "Usage: summary {game} {user} {file}" << std::endl;
                continue;
            }
            protocol.saveSummary(game, user, file);
        }
        else if (!protocol.isConnectedToSocket()) {
            std::cout << "Please login first" << std::endl;
            continue;
        }
        else if (command == "join") {
            std::string game;
            if (!(ss >> game)) continue;
            protocol.sendJoin(game);
        }
        else if (command == "report") {
            std::string path;
            if (!(ss >> path)) continue;
            std::ifstream f(path);
            if (!f.good()) {
                std::cout << "File not found: " << path << std::endl;
                continue;
            }
            protocol.sendReport(path);
        }
        else if (command == "logout") {
            protocol.sendLogout();
        }
    }

    if (socketThread.joinable()) socketThread.join();
    return 0;
}