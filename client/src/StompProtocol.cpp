#include "../include/StompProtocol.h"
#include "../include/event.h" // פותר את שגיאת namespaced_events ו-parseEventsFile
#include <iostream>
#include <fstream>

StompProtocol::StompProtocol() 
    : receiptCounter(0),            // 1. ראשון ב-Header
      pendingReceipts(),            // 2. שני
      topicIds(),                   // 3. שלישי
      connectionHandler(nullptr),    // 4. רביעי
      currentUser(""),              // 5. חמישי (חדש)
      connected(false),             // 6. שישי
      shouldTerminate(false),       // 7. שביעי
      gameReports() {}              // 8. שמיני

StompProtocol::~StompProtocol() { if (connectionHandler) delete connectionHandler; }

bool StompProtocol::connect(std::string host, short port, std::string user, std::string pass) {
    if (connectionHandler) delete connectionHandler;
    connectionHandler = new ConnectionHandler(host, port);

    if (!connectionHandler->connect()) {
        std::cout << "Could not connect to server" << std::endl;
        return false;
    }

    // פריים CONNECT תקני לפי עמוד 3
    std::string frame = "CONNECT\n"
                        "accept-version:1.2\n"
                        "host:stomp.cs.bgu.ac.il\n"
                        "login:" + user + "\n"
                        "passcode:" + pass + "\n"
                        "\n";

    connectionHandler->sendFrame(frame);

    std::string response;
    if (!connectionHandler->getFrame(response)) return false;

    // הדפסת הפריים שהתקבל לפי הדרישה
    std::cout << "\nFrame received from server:\n---\n" << response << "\n---\n" << std::endl;

    if (response.find("CONNECTED") != std::string::npos) {
        connected = true;
        std::cout << "Login successful" << std::endl;
        return true;
    }
    
    // טיפול בשגיאות (סיסמה שגויה או משתמש מחובר)
    if (response.find("User already logged in") != std::string::npos) std::cout << "User already logged in" << std::endl;
    else if (response.find("Wrong password") != std::string::npos) std::cout << "Wrong password" << std::endl;
    else std::cout << "Login failed" << std::endl;
    
    return false;
}

std::string StompProtocol::processKeyboardCommand(const std::string& input) {
    std::istringstream iss(input);
    std::string command;
    iss >> command;

    if (command == "report") {
        std::string user, team, eventType, desc;
        int time; bool beforeHalf;
        if (!(iss >> user >> team >> eventType >> time >> beforeHalf)) return "";
        std::getline(iss, desc);
        if (!desc.empty() && desc[0] == ' ') desc = desc.substr(1);

        // תיקון image_5bd2c4: שימוש בחץ (->) כי זה מצביע
        std::string sql = "INSERT INTO events (user, team, eventType, time, beforeHalftime, description) VALUES ('" 
                          + user + "', '" + team + "', '" + eventType + "', " 
                          + std::to_string(time) + ", " + (beforeHalf ? "1" : "0") + ", '" + desc + "')";
        
        if (connectionHandler) connectionHandler->sendFrame(sql);

        // עדכון מקומי ל-Summary
        GameEventReport report;
        report.user = user; report.eventName = eventType;
        report.time = time; report.description = desc;
        report.teamA = team;

        gameReports["/Germany_Japan"].push_back(report);
    }
    return "";
}

void StompProtocol::processServerFrame(const std::string& frame) {
    std::string command, body;
    std::unordered_map<std::string, std::string> headers;
    parseFrame(frame, command, headers, body);

    if (command == "RECEIPT") {
        int rId = std::stoi(headers["receipt-id"]);
        if (pendingReceipts.count(rId)) {
            std::string action = pendingReceipts[rId];
            std::cout << action << std::endl;
            if (action == "Logout") {
                connected = false;
                shouldTerminate = true; // עוצר את הלולאה ומונע את שגיאות ה-recv
                if (connectionHandler) connectionHandler->close();
            }
            pendingReceipts.erase(rId);
        }
    } 
    else if (command == "MESSAGE") {
        // ה-destination ב-MESSAGE קובע לאיזה משחק לשייך את הדיווח
        handleMessageFrame(headers["destination"], body);
    }
}

void StompProtocol::handleMessageFrame(std::string topic, std::string body) {
    std::stringstream ss(body);
    std::string line;
    GameEventReport report;
    
    // קריאת גוף ההודעה לפי הפורמט בעמוד 16
    while (std::getline(ss, line) && !line.empty()) {
        if (line.find("user: ") == 0) report.user = line.substr(6);
        else if (line.find("team a: ") == 0) report.teamA = line.substr(8);
        else if (line.find("team b: ") == 0) report.teamB = line.substr(8);
        else if (line.find("event name: ") == 0) report.eventName = line.substr(12);
        else if (line.find("time: ") == 0) report.time = std::stoi(line.substr(6));
        else if (line.find("description:") == 0) {
            std::string desc;
            while (std::getline(ss, line)) desc += line + "\n";
            report.description = desc;
        }
        else {
            // עיבוד עדכונים סטטיסטיים (goals, possession וכו')
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string val = line.substr(pos + 1);
                if (key[0] == ' ') key.erase(0, 1);
                if (val[0] == ' ') val.erase(0, 1);
                report.updates[key] = val;
            }
        }
    }
    gameReports[topic].push_back(report);
}

void StompProtocol::processMessageBody(std::stringstream& bodyStream, std::string destination) {
    GameEventReport report;
    std::string line;
    while (std::getline(bodyStream, line)) {
        size_t pos = line.find(':');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        if (key == "user") report.user = value;
        else if (key == "team a") report.teamA = value;
        else if (key == "team b") report.teamB = value;
        else if (key == "event name") report.eventName = value;
        else if (key == "time") report.time = std::stoi(value);
        else if (key == "description") {
            // התיאור עשוי להיות רב-שורתי
            std::string desc;
            while (std::getline(bodyStream, line)) desc += line + "\n";
            report.description = desc;
        }
    }
    gameReports[destination].push_back(report);
}

void StompProtocol::runSocketListener() {
    if (!connected || shouldTerminate) return; // בדיקה לפני קריאה

    std::string frame;
    if (connectionHandler && connectionHandler->getFrame(frame)) {
        if (!frame.empty()) {
            std::cout << "\nFrame received from server:\n---\n" << frame << "\n---\n" << std::endl;
            processServerFrame(frame);
        }
    } else {
        // אם getFrame נכשל, הסוקט כנראה נסגר - עוצרים את המאזין
        connected = false;
        shouldTerminate = true;
    }
}

bool StompProtocol::shouldTerminateClient() const { return shouldTerminate; }
bool StompProtocol::isConnectedToSocket() const { return connected; }

void StompProtocol::sendJoin(std::string game) {
    if (connectionHandler && connected) {
        int rId = receiptCounter++;
        std::string topic = (game[0] == '/') ? game : "/" + game;
        topicIds[1] = topic; // שמירת ה-ID לזיהוי MESSAGE [cite: 147-152]

        std::string stompFrame = "SUBSCRIBE\n"
                                 "destination:" + topic + "\n"
                                 "id:1\n"
                                 "receipt:" + std::to_string(rId) + "\n" // קריטי לקבלת פריים חזרה
                                 "\n";
        
        pendingReceipts[rId] = "Joined channel " + game;
        connectionHandler->sendFrame(stompFrame);
    }
}

void StompProtocol::sendLogout() {
    if (connectionHandler && connected) {
        int rId = receiptCounter++;
        std::string frame = "DISCONNECT\n"
                            "receipt:" + std::to_string(rId) + "\n" // קריטי לסגירה נקייה [cite: 164-165, 485]
                            "\n";
        
        pendingReceipts[rId] = "Logout";
        connectionHandler->sendFrame(frame);
    }
}


void StompProtocol::parseFrame(const std::string& frame, std::string& command, std::unordered_map<std::string, std::string>& headers, std::string& body) {
    std::istringstream input(frame);
    std::getline(input, command);
    std::string line;
    while (std::getline(input, line) && !line.empty() && line != "\r") {
        size_t pos = line.find(':');
        if (pos != std::string::npos) headers[line.substr(0, pos)] = line.substr(pos + 1);
    }
    std::getline(input, body, '\0');
}


void StompProtocol::saveSummary(std::string game, std::string user, std::string file) {
    std::string topic = (game[0] == '/') ? game : "/" + game;
    std::ofstream outFile(file);
    if (!outFile.is_open()) return;

    size_t underscore = game.find('_');
    std::string tA = game.substr(0, underscore);
    if (tA[0] == '/') tA.erase(0,1);
    std::string tB = game.substr(underscore + 1);
    
    outFile << tA << " vs " << tB << "\nGame stats:\nGeneral stats:\n";

    std::map<std::string, std::string> stats; // Map ממיין אוטומטית לפי ABC 
    for (auto& report : gameReports[topic]) {
        if (report.user == user) {
            for (auto const& it : report.updates) {
                stats[it.first] = it.second;
            }
        }
    }

    for (auto const& it : stats) {
        outFile << "    " << it.first << ": " << it.second << "\n";
    }

    outFile << "Game event reports:\n";
    for (auto& report : gameReports[topic]) {
        if (report.user == user) {
            outFile << report.time << " - " << report.eventName << ":\n\n" << report.description << "\n\n";
        }
    }
    outFile.close();
}

void StompProtocol::sendReport(std::string path) {
    names_and_events events_data = parseEventsFile(path); 
    
    for (const auto& ev : events_data.events) {
        std::string topic = "/" + ev.get_team_a_name() + "_" + ev.get_team_b_name();

        // בניית גוף ההודעה (Body) בדיוק לפי הפורמט בעמוד 16 [cite: 382-401]
        std::string body = "user: " + events_data.team_a_name + "\n"
                           "team a: " + ev.get_team_a_name() + "\n"
                           "team b: " + ev.get_team_b_name() + "\n"
                           "event name: " + ev.get_name() + "\n"
                           "time: " + std::to_string(ev.get_time()) + "\n"
                           "description:\n" + ev.get_discription() + "\n";

        // פריים SEND תקני [cite: 382-383]
        std::string stompFrame = "SEND\n"
                                 "destination:" + topic + "\n"
                                 "\n" + body;

        connectionHandler->sendFrame(stompFrame);

        // שמירת האירוע בזיכרון המקומי לטובת פקודת ה-summary [cite: 378-379, 471]
        GameEventReport report;
        report.user = events_data.team_a_name;
        report.eventName = ev.get_name();
        report.time = ev.get_time();
        report.description = ev.get_discription();
        report.teamA = ev.get_team_a_name();
        report.teamB = ev.get_team_b_name();
        for (auto const& x : ev.get_game_updates()) report.updates[x.first] = x.second;
        for (auto const& x : ev.get_team_a_updates()) report.updates[x.first] = x.second;
        for (auto const& x : ev.get_team_b_updates()) report.updates[x.first] = x.second;
        
        gameReports[topic].push_back(report);
    }
}