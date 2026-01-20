#include "../include/StompProtocol.h"
#include  "../include/event.h"
#include <sstream>
#include <iostream>
#include <fstream>

StompProtocol::StompProtocol() : 
    connectionHandler(nullptr), 
    isConnected(false), 
    subIdCounter(0), 
    receiptIdCounter(0), 
    disconnectReceiptId(-1), 
    shouldTerminate(false), 
    userName(""), 
    topicToSubId(), 
    gameReports(),
    receiptToCommand(), 
    waitingForReceipt(false)
{}

StompProtocol::~StompProtocol() {
    if (connectionHandler != nullptr) delete connectionHandler;
}

bool StompProtocol::shouldTerminateClient() const {
    return shouldTerminate;
}

bool StompProtocol::isWaitingForReceipt() const {
    return waitingForReceipt;
}

bool StompProtocol::connect(std::string host, short port, std::string user, std::string pass) {
    connectionHandler = new ConnectionHandler(host, port);
    if (!connectionHandler->connect()) {
        std::cout << "Could not connect to server" << std::endl;
        return false;
    }
    this->userName = user;
    // שליחת CONNECT עם NULL בסוף
    std::string connectFrame = "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:" + user + "\npasscode:" + pass + "\n\n";
    connectFrame.push_back('\0'); // הבטחה שהתו קיים
    if (!connectionHandler->sendFrame(connectFrame)) return false;

    std::string answer;
    if (connectionHandler->getFrame(answer)) {
        std::cout << "Server sent:\n" << answer << std::endl << std::flush;
        if (answer.find("CONNECTED") != std::string::npos) {
            this->isConnected = true;
            return true;
        }
    }
    return false;
}

void StompProtocol::sendJoin(std::string gameName) {
    int subId = subIdCounter++;
    int receiptId = receiptIdCounter++;
    
    topicToSubId[gameName] = subId;
    receiptToCommand[receiptId] = "JOIN " + gameName; // כעת המשתנה מוכר
    waitingForReceipt = true; // חוסם קלט עד לקבלת אישור השרת

    std::string frame = "SUBSCRIBE\ndestination:/" + gameName + "\nid:" + std::to_string(subId) + 
                        "\nreceipt:" + std::to_string(receiptId) + "\n\n";
    frame.push_back('\0');
    connectionHandler->sendFrame(frame);
}

void StompProtocol::sendLogout() {
    int receiptId = receiptIdCounter++;
    receiptToCommand[receiptId] = "LOGOUT";
    waitingForReceipt = true;

    std::string frame = "DISCONNECT\nreceipt:" + std::to_string(receiptId) + "\n\n";
    frame.push_back('\0');
    connectionHandler->sendFrame(frame);
}

// src/StompProtocol.cpp

void StompProtocol::runSocketListener() {
    while (!shouldTerminate) {
        std::string answer;
        if (!connectionHandler->getFrame(answer)) break; 
        
        // הדפסה לטרמינל
        std::cout << "Server sent:\n" << answer << std::endl;
        
        // חובה: עיבוד הפריים ושמירתו ב-gameReports לצורך ה-summary
        processServerFrame(answer); 
    }
}

void StompProtocol::sendReport(std::string jsonFile) {
    names_and_events nne = parseEventsFile(jsonFile);
    // הוספת הלוכסן כבר כאן כדי להיות עקביים מול השרת
    std::string destination = "/" + nne.team_a_name + "_" + nne.team_b_name;

    for (const auto& event : nne.events) {
        std::string frame = "SEND\ndestination:" + destination + "\n\n";
        frame += "user: " + userName + "\n";
        frame += "team a: " + event.get_team_a_name() + "\n";
        frame += "team b: " + event.get_team_b_name() + "\n";
        frame += "event name: " + event.get_name() + "\n";
        frame += "time: " + std::to_string(event.get_time()) + "\n";
        
        frame += "general game updates:\n";
        for (auto const& x : event.get_game_updates()) 
            frame += "    " + x.first + ": " + x.second + "\n";
            
        frame += "team a updates:\n";
        for (auto const& x : event.get_team_a_updates()) 
            frame += "    " + x.first + ": " + x.second + "\n";
            
        frame += "team b updates:\n";
        for (auto const& x : event.get_team_b_updates()) 
            frame += "    " + x.first + ": " + x.second + "\n";
            
        frame += "description:\n" + event.get_discription() + "\0"; 
        
        connectionHandler->sendFrame(frame);
    }
}

void StompProtocol::processServerFrame(std::string frame) {
    std::stringstream str(frame);
    std::string line, command;
    std::map<std::string, std::string> headers;
    
    // 1. חילוץ הפקודה
    if (!std::getline(str, command)) return;
    if (!command.empty() && command.back() == '\r') command.pop_back();

    // 2. חילוץ הדרים עד לשורה הריקה
    while (std::getline(str, line) && !line.empty() && line != "\r") {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            headers[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }

    // 3. לוגיקה לפי סוג הפקודה
    if (command == "CONNECTED") {
        std::cout << "Login successful" << std::endl;
    } 
    else if (command == "RECEIPT") {
        if (headers.count("receipt-id")) {
            int rId = std::stoi(headers["receipt-id"]);
            std::string originalCommand = receiptToCommand[rId];
            
            if (originalCommand.find("JOIN") == 0) 
                std::cout << "Joined channel " << originalCommand.substr(5) << std::endl;
            else if (originalCommand.find("EXIT") == 0)
                std::cout << "Exited channel " << originalCommand.substr(5) << std::endl;
            
            if (originalCommand == "LOGOUT") {
                shouldTerminate = true;
            }
            waitingForReceipt = false; 
        }
    } 
    else if (command == "MESSAGE") {
        // שליחת שארית ה-stream (גוף ההודעה) לעיבוד
        processMessageBody(str, headers["destination"]);
    } 
    else if (command == "ERROR") {
        std::cout << "Error from server: " << headers["message"] << std::endl;
        shouldTerminate = true;
    }
}

void StompProtocol::processMessageBody(std::stringstream& bodyStream, std::string destination) {
    GameEventReport report;
    std::string line;

    while (std::getline(bodyStream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);

        // חילוץ שמות הקבוצות - קריטי לשיוך הסטטיסטיקה ב-saveSummary
        if (key == "user") report.user = value;
        else if (key == "team a") report.teamA = value;
        else if (key == "team b") report.teamB = value;
        else if (key == "event name") report.eventName = value;
        else if (key == "time") report.time = std::stoi(value);
        else if (key == "description") {
            std::string desc;
            while (std::getline(bodyStream, line)) desc += line + "\n";
            report.description = desc;
            break;
        }
        else if (line.find("    ") == 0) { // ניקוי מפתחות סטטיסטיקה מרווחים
            std::string cleanKey = key;
            cleanKey.erase(0, cleanKey.find_first_not_of(" "));
            report.updates[cleanKey] = value;
        }
    }
    gameReports[destination].push_back(report);
}

void StompProtocol::saveSummary(std::string gameName, std::string user, std::string fileName) {
    std::string topic = (gameName[0] == '/') ? gameName : "/" + gameName;
    if (gameReports.find(topic) == gameReports.end()) return;

    std::vector<GameEventReport>& reports = gameReports[topic];
    std::ofstream outFile(fileName);
    
    // חילוץ שמות מדינות מהפקודה
    size_t underscore = gameName.find('_');
    std::string teamAName = gameName.substr(0, underscore);
    if (teamAName[0] == '/') teamAName.erase(0, 1);
    std::string teamBName = gameName.substr(underscore + 1);

    std::map<std::string, std::string> gen, updA, updB;
    std::vector<GameEventReport> userEvents;

    for (const auto& r : reports) {
        if (r.user == user) {
            userEvents.push_back(r);
            for (auto const& x : r.updates) {
                // שיוך אקומולטיבי (הערך האחרון דורס את הקודם)
                if (r.teamA == teamAName) updA[x.first] = x.second;
                else if (r.teamB == teamBName) updB[x.first] = x.second;
                else gen[x.first] = x.second; // סטטיסטיקה כללית
            }
        }
    }

    // פורמט הדפסה סופי (PDF עמוד 18)
    outFile << teamAName << " vs " << teamBName << "\nGame stats:\nGeneral stats:\n";
    for (auto const& x : gen) outFile << "    " << x.first << ": " << x.second << "\n";
    outFile << teamAName << " stats:\n";
    for (auto const& x : updA) outFile << "    " << x.first << ": " << x.second << "\n";
    outFile << teamBName << " stats:\n";
    for (auto const& x : updB) outFile << "    " << x.first << ": " << x.second << "\n";

    outFile << "Game event reports:\n";