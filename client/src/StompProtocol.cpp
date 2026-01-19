#include "../include/StompProtocol.h"
#include  "../include/event.h"
#include <sstream>
#include <iostream>
#include <fstream>

StompProtocol::StompProtocol() : 
    connectionHandler(nullptr), isConnected(false), subIdCounter(0), 
    receiptIdCounter(0), disconnectReceiptId(-1), shouldTerminate(false), 
    userName(""), topicToSubId(), gameReports() {}

StompProtocol::~StompProtocol() {
    if (connectionHandler != nullptr) delete connectionHandler;
}

bool StompProtocol::shouldTerminateClient() const {
    return shouldTerminate;
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
    // חובה NULL בסוף כדי שהריאקטור יגיב
    std::string frame = "SUBSCRIBE\ndestination:/" + gameName + "\nid:" + std::to_string(subId) + "\nreceipt:" + std::to_string(receiptId) + "\n\n";
    frame.push_back('\0'); // המפתח לשחרור השתיקה בריאקטור
    connectionHandler->sendFrame(frame);
}


void StompProtocol::sendLogout() {
    disconnectReceiptId = receiptIdCounter++;
    // שליחת DISCONNECT עם NULL בסוף
    std::string frame = "DISCONNECT\nreceipt:" + std::to_string(disconnectReceiptId) + "\n\n\0";
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
    names_and_events nameEvents = parseEventsFile(jsonFile);
    std::string topic = "/" + nameEvents.team_a_name + "_" + nameEvents.team_b_name;

    for (const auto& event : nameEvents.events) {
        // Headers - שים לב ל-\n\n שיוצר את השורה הריקה הנדרשת
        std::string frame = "SEND\ndestination:" + topic + "\n\n";
        
        // Body - שדות key:value נקיים ללא כפילויות
        frame += "user:" + userName + "\n";
        frame += "event name:" + event.get_name() + "\n";
        frame += "time:" + std::to_string(event.get_time()) + "\n";
        frame += "team a:" + event.get_team_a_name() + "\n";
        frame += "team b:" + event.get_team_b_name() + "\n";
        
        for (auto const& x : event.get_game_updates()) {
            frame += x.first + ":" + x.second + "\n";
        }
        
        // סגירה עם התיאור ותו ה-NULL
        frame += "description:" + event.get_discription() + "\0"; 

        connectionHandler->sendFrame(frame);
    }
}

void StompProtocol::processServerFrame(std::string frame) {
    std::stringstream str(frame);
    std::string line, command;
    std::getline(str, command);
    // ניקוי \r מהפקודה (חשוב לריאקטור)
    if (!command.empty() && command.back() == '\r') command.pop_back();

    if (command == "MESSAGE") {
        std::string destination;
        // דילוג על Headers עד לשורה ריקה
        while (std::getline(str, line) && !line.empty() && line != "\r") {
            if (line.find("destination:") == 0) {
                destination = line.substr(12);
                if (!destination.empty() && destination.back() == '\r') destination.pop_back();
            }
        }

        GameEventReport report;
        // עיבוד גוף ההודעה - כאן נבנה הסיכום
        while (std::getline(str, line)) {
            if (line.empty() || line == "\r" || line[0] == '\0') continue;
            if (line.back() == '\r') line.pop_back(); // ניקוי \r מכל שורת נתונים

            if (line.find("description:") == 0) {
                report.description = line.substr(12);
                // המשך קריאת תיאור אם הוא רב-שורתי
                while(std::getline(str, line) && !line.empty() && line[0] != '\0') {
                    if (line.back() == '\r') line.pop_back();
                    report.description += "\n" + line;
                }
                break; 
            }

            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos), val = line.substr(pos + 1);
                if (key == "user") report.user = val;
                else if (key == "team a") report.teamA = val;
                else if (key == "team b") report.teamB = val;
                else if (key == "event name") report.eventName = val;
                else if (key == "time") report.time = val;
                else report.updates[key] = val;
            }
        }
        if (!destination.empty()) gameReports[destination].push_back(report);
    }
}

void StompProtocol::saveSummary(std::string gameName, std::string user, std::string fileName) {
    std::string topic = "/" + gameName;
    std::ofstream outFile(fileName);
    if (!outFile.is_open()) return;

    std::vector<GameEventReport> reports = gameReports[topic];
    std::string teamA = "", teamB = "";
    std::map<std::string, std::string> general, tA_upd, tB_upd;

    for (const auto& report : reports) {
        if (report.user == user) {
            if(teamA == "") teamA = report.teamA;
            if(teamB == "") teamB = report.teamB;
            
            for (auto const& x : report.updates) {
                // בדיקת שייכות הסטטיסטיקה לקבוצה או לכללי
                if (x.first.find(teamA) != std::string::npos || x.first.find("team a") != std::string::npos) 
                    tA_upd[x.first] = x.second;
                else if (x.first.find(teamB) != std::string::npos || x.first.find("team b") != std::string::npos) 
                    tB_upd[x.first] = x.second;
                else 
                    general[x.first] = x.second; // כאן ייכנסו active ו-before halftime
            }
        }
    }

    // הדפסה בפורמט התקני
    outFile << teamA << " vs " << teamB << "\nGame stats:\nGeneral stats:\n";
    for (auto const& x : general) outFile << "    " << x.first << ": " << x.second << "\n";
    outFile << "Team a stats:\n";
    for (auto const& x : tA_upd) outFile << "    " << x.first << ": " << x.second << "\n";
    outFile << "Team b stats:\n";
    for (auto const& x : tB_upd) outFile << "    " << x.first << ": " << x.second << "\n";

    outFile << "Game event reports:\n";
    for (const auto& report : reports) {
        if (report.user == user) {
            outFile << report.time << " - " << report.eventName << ":\n\n" << report.description << "\n\n";
        }
    }
    outFile.close(); // חובה לסגור את הקובץ כדי שהנתונים ייכתבו לדיסק
}