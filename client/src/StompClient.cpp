#include <iostream>
#include <thread>
#include <sstream>
#include "../include/StompProtocol.h"

int main() {
    StompProtocol protocol;

    std::thread socketListener;
    bool listenerStarted = false;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "login") {
            if (protocol.isConnectedToSocket()) {
                std::cout << "Already logged in. Logout first." << std::endl;
                continue;
            }

            std::string hostPort, user, pass;
            if (!(iss >> hostPort >> user >> pass)) {
                std::cout << "Usage: login {host:port} {user} {password}" << std::endl;
                continue;
            }

            size_t colonPos = hostPort.find(':');
            if (colonPos == std::string::npos) {
                std::cout << "Invalid host:port format" << std::endl;
                continue;
            }

            std::string host = hostPort.substr(0, colonPos);
            short port = std::stoi(hostPort.substr(colonPos + 1));

            if (protocol.connect(host, port, user, pass)) {
                if (!listenerStarted) {
                    listenerStarted = true;
                    socketListener = std::thread([&protocol]() {
                        while (protocol.isConnectedToSocket() &&
                               !protocol.shouldTerminateClient()) {
                            protocol.runSocketListener();
                        }
                    });
                }
            }
            continue;
        }

        if (command == "logout") {
            protocol.sendLogout();
            break;
        }

        if (!protocol.isConnectedToSocket()) {
            std::cout << "Not connected. Please login first." << std::endl;
            continue;
        }

        if (command == "join") {
            std::string game;
            if (iss >> game)
                protocol.sendJoin(game);
        }
        else if (command == "report") {
            std::string path;
            if (iss >> path)
                protocol.sendReport(path);
        }
        else if (command == "summary") {
            std::string game, user, file;
            if (iss >> game >> user >> file) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                protocol.saveSummary(game, user, file);
            } else {
                    std::cout << "Usage: summary {game} {user} {file}" << std::endl;
                    }
        }

        else {
            std::cout << "Unknown command: " << command << std::endl;
        }
    }

    if (listenerStarted && socketListener.joinable()) {
        socketListener.join();
    }

    return 0;
}
