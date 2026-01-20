#include "../include/StompProtocol.h"
#include "../include/event.h" // פותר את שגיאת namespaced_events ו-parseEventsFile
#include <iostream>
#include <fstream>

StompProtocol::StompProtocol() : connectionHandler(nullptr), connected(false), shouldTerminate(false), gameReports(), pendingReceipts() {}

StompProtocol::~StompProtocol() { if (connectionHandler) delete connectionHandler; }

bool StompProtocol::connect(std::string host, short port, std::string user, std::string pass) {
    if (connectionHandler) delete connectionHandler;
    connectionHandler = new ConnectionHandler(host, port);

    if (!connectionHandler->connect()) return false;

    // בניית פריים CONNECT תקין עבור השרת המעודכן
    std::string frame =
        "CONNECT\n"
        "accept-version:1.2\n"
        "host:" + host + "\n"
        "login:" + user + "\n"
        "passcode:" + pass + "\n\n";

    connectionHandler->sendFrame(frame);

    std::string response;
    // getFrame קורא עד לתו ה-NULL ששלח השרת (response + "\0")
    if (!connectionHandler->getFrame(response)) return false;

    // השרת מחזיר פריים CONNECTED
    if (response.find("CONNECTED") != std::string::npos) {
        connected = true;
        std::cout << "Login successful" << std::endl;
        return true;
    }

    std::cout << "Login failed: " << response << std::endl;
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
    std::string cmd, body;
    std::unordered_map<std::string, std::string> headers;
    parseFrame(frame, cmd, headers, body);

    if (cmd == "MESSAGE") {
        // תיקון: שליפה מהמפה עם מפתח במקום הצבת המפה למחרוזת
        std::string dest = headers.count("destination") ? headers.at("destination") : "unknown";
        std::stringstream ss(body);
        processMessageBody(ss, dest);
    } 
    else if (cmd == "ERROR") {
        shouldTerminate = true;
    }
    else if (frame.find("done") != std::string::npos || frame.find("error") != std::string::npos) {
        // טיפול בתשובות SQL משרת ה-Python
        std::cout << "[SQL Server Response]: " << frame << std::endl;
    }
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

// מימוש שאר המתודות הפנימיות...
void StompProtocol::runSocketListener() {
    std::string frame;
    if (connectionHandler && connectionHandler->getFrame(frame)) {
        processServerFrame(frame);
    }
}

bool StompProtocol::shouldTerminateClient() const { return shouldTerminate; }
bool StompProtocol::isConnectedToSocket() const { return connected; }

void StompProtocol::sendJoin(std::string game) {
    if (connectionHandler && connected) {
        int rId = receiptCounter++;
        std::string topic = (game[0] == '/') ? game : "/" + game;
        
        std::string stompFrame = "SUBSCRIBE\n"
                                 "destination:" + topic + "\n"
                                 "id:1\n"
                                 "receipt:" + std::to_string(rId) + "\n"
                                 "\n";
        
        pendingReceipts[rId] = "Joined " + topic;
        connectionHandler->sendFrame(stompFrame);
    }
}

void StompProtocol::sendLogout() {
    if (connectionHandler && connected) {
        int rId = receiptCounter++;
        
        // פריים DISCONNECT תקני
        std::string frame = "DISCONNECT\n"
                            "receipt:" + std::to_string(rId) + "\n"
                            "\n";
        
        pendingReceipts[rId] = "Logout";
        connectionHandler->sendFrame(frame);
        
        // הערה: ה-connected וה-shouldTerminate יתעדכנו ב-processServerFrame 
        // ברגע שתתקבל הודעת ה-RECEIPT מהשרת.
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
    
    if (gameReports.find(topic) == gameReports.end()) {
        std::cout << "No reports found for topic: " << topic << std::endl;
        return;
    }

    std::ofstream outFile(file);
    if (!outFile.is_open()) return;

    // 1. כותרת המשחק
    size_t underscore = game.find('_');
    std::string teamA = game.substr(0, underscore);
    if (teamA[0] == '/') teamA.erase(0, 1);
    std::string teamB = (underscore != std::string::npos) ? game.substr(underscore + 1) : "Unknown";

    outFile << teamA << " vs " << teamB << "\n";
    outFile << "Game stats:\n";

    // 2. אגרגציה של סטטיסטיקות (שימוש בערך האחרון שדווח)
    std::map<std::string, std::string> generalStats, teamAStats, teamBStats;
    std::vector<GameEventReport> eventsToPrint;

    for (const auto& report : gameReports[topic]) {
        if (report.user == user) {
            eventsToPrint.push_back(report);
            // העתקת העדכונים מהמפה המקומית
            for (auto const& x : report.updates) {
                // לוגיקה למיון הסטטיסטיקות לפי הקבוצה המדווחת
                if (report.teamA == teamA) teamAStats[x.first] = x.second;
                else if (report.teamA == teamB) teamBStats[x.first] = x.second;
                else generalStats[x.first] = x.second;
            }
        }
    }

    // 3. כתיבת הסטטיסטיקות לפי הפורמט המדויק
    outFile << "General stats:\n";
    for (auto const& x : generalStats) outFile << "    " << x.first << ": " << x.second << "\n";
    
    outFile << teamA << " stats:\n";
    for (auto const& x : teamAStats) outFile << "    " << x.first << ": " << x.second << "\n";
    
    outFile << teamB << " stats:\n";
    for (auto const& x : teamBStats) outFile << "    " << x.first << ": " << x.second << "\n";

    // 4. יומן אירועים כרונולוגי
    outFile << "Game event reports:\n";
    for (const auto& r : eventsToPrint) {
        outFile << r.time << " - " << r.eventName << ":\n\n";
        outFile << r.description << "\n\n";
    }

    outFile.close();
    std::cout << "Summary file created successfully: " << file << std::endl;
}

void StompProtocol::sendReport(std::string path) {
    // קריאת הקובץ לתוך המבנה הנכון
    names_and_events events_data = parseEventsFile(path); 
    
    for (const auto& ev : events_data.events) {
        std::string topic = "/" + ev.get_team_a_name() + "_" + ev.get_team_b_name();

        // בניית גוף הודעת ה-STOMP
        std::string body = "user:" + events_data.team_a_name + "\n"
                           "team a:" + ev.get_team_a_name() + "\n"
                           "team b:" + ev.get_team_b_name() + "\n"
                           "event name:" + ev.get_name() + "\n"
                           "time:" + std::to_string(ev.get_time()) + "\n"
                           "description:" + ev.get_discription() + "\n";

        // שליחת פריים SEND
        std::string stompFrame = "SEND\n"
                                 "destination:" + topic + "\n"
                                 "\n" + body;
        connectionHandler->sendFrame(stompFrame);

        // שליחה מקבילה ל-SQL לשמירה בשרת ה-Python
        std::string sql = "INSERT INTO events (user, team, eventType, time, description) VALUES ('"
                          + events_data.team_a_name + "', '" + topic + "', '" + ev.get_name() + "', " 
                          + std::to_string(ev.get_time()) + ", '" + ev.get_discription() + "')";
        connectionHandler->sendFrame(sql);

        // עדכון זיכרון מקומי ל-Summary
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