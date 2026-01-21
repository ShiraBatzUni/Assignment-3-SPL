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

std::string StompProtocol::normalizeTopic(std::string topic) {
    if (topic.empty()) return "/";
    if (topic[0] != '/') return "/" + topic;
    return topic;
}

bool StompProtocol::connect(std::string host, short port, std::string user, std::string pass) {
    if (connectionHandler) delete connectionHandler;
    connectionHandler = new ConnectionHandler(host, port);

    if (!connectionHandler->connect()) {
        std::cout << "Could not connect to server" << std::endl;
        return false;
    }

    // שליחת פריים CONNECT
    std::string frame = "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:" + user + "\npasscode:" + pass + "\n\n";
    if (!connectionHandler->sendFrame(frame)) return false;

    std::string response;
    if (!connectionHandler->getFrame(response)) return false;
    std::cout << "\nFrame received from server:\n---\n" << response << "\n---\n" << std::endl;
    // בדיקה אם התקבל אישור מהשרת
    if (response.find("CONNECTED") != std::string::npos) {
        this->connected = true; // עדכון המשתנה שמאפשר את שאר הפקודות
        this->currentUser = user; // שמירה לטובת ה-Summary
        std::cout << "Login successful" << std::endl;
        return true;
    }
    
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
    
    // פירוק הפריים למרכיביו
    parseFrame(frame, command, headers, body);

    if (command == "RECEIPT") {
        if (headers.count("receipt-id")) {
            int rId = std::stoi(headers["receipt-id"]);
            if (pendingReceipts.count(rId)) {
                std::string action = pendingReceipts[rId];
                
                // טיפול ייעודי בניתוק (Logout)
                if (action == "Logout") {
                    std::cout << "Logout successful. Closing connection..." << std::endl;
                    connected = false;
                    shouldTerminate = true;
                } else {
                    std::cout << "Receipt received for: " << action << std::endl;
                }
                pendingReceipts.erase(rId);
            }
        }
    } 
    else if (command == "MESSAGE") {
        std::string topic = headers["destination"];
        // עיבוד ההודעה ושמירתה ללא כפילויות
        handleMessageFrame(topic, body);
    }
    else if (command == "ERROR") {
        std::cout << "Server Error: " << headers["message"] << std::endl;
        std::cout << "Closing connection due to error." << std::endl;
        connected = false;
        shouldTerminate = true;
    }
}

void StompProtocol::handleMessageFrame(std::string topic, std::string body) {
    std::string normTopic = normalizeTopic(topic);
    std::stringstream ss(body);
    std::string line;

    GameEventReport report;

    enum class Section { NONE, GENERAL, TEAM_A, TEAM_B, DESCRIPTION };
    Section section = Section::NONE;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (line == "general game updates:") { section = Section::GENERAL; continue; }
        if (line == "team a updates:")       { section = Section::TEAM_A;  continue; }
        if (line == "team b updates:")       { section = Section::TEAM_B;  continue; }
        if (line == "description:")          { section = Section::DESCRIPTION; continue; }

        // שדות כותרת בפורמט key: value
        size_t pos = line.find(": ");
        if (pos != std::string::npos && section != Section::DESCRIPTION) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 2);

            if (key == "user") report.user = val;
            else if (key == "team a") report.teamA = val;
            else if (key == "team b") report.teamB = val;
            else if (key == "event name") report.eventName = val;
            else if (key == "time") report.time = std::stoi(val);
            continue;
        }

        // עדכונים בתוך מקטעים
        if (section == Section::GENERAL || section == Section::TEAM_A || section == Section::TEAM_B) {
            size_t p = line.find(": ");
            if (p != std::string::npos) {
                std::string k = line.substr(0, p);
                std::string v = line.substr(p + 2);

                if (section == Section::GENERAL) report.generalUpdates[k] = v;
                else if (section == Section::TEAM_A) report.teamAUpdates[k] = v;
                else if (section == Section::TEAM_B) report.teamBUpdates[k] = v;
            }
            continue;
        }

        // תיאור (יכול להיות multi-line)
        if (section == Section::DESCRIPTION) {
            report.description += line + "\n";
        }
    }

    gameReports[normTopic].push_back(report);
}


void StompProtocol::sendReport(std::string path) {
    try {
        names_and_events events_data = parseEventsFile(path); 
        
        for (const auto& ev : events_data.events) {
            std::string topic = normalizeTopic(ev.get_team_a_name() + "_" + ev.get_team_b_name());
            std::string body =
            "user: " + currentUser + "\n"
            "team a: " + ev.get_team_a_name() + "\n"
            "team b: " + ev.get_team_b_name() + "\n"
            "event name: " + ev.get_name() + "\n"
            "time: " + std::to_string(ev.get_time()) + "\n"
            "general game updates:\n";

            body += "general game updates:\n";
            for (const auto& x : ev.get_game_updates()) {
                body += x.first + ": " + x.second + "\n";
                }

            body += "team a updates:\n";
            for (const auto& x : ev.get_team_a_updates()) {
                body += x.first + ": " + x.second + "\n";
                }

            body += "team b updates:\n";
            for (const auto& x : ev.get_team_b_updates()) {
                body += x.first + ": " + x.second + "\n";
                }

            body += "description:\n";
            body += ev.get_discription() + "\n";

            std::string stompFrame = "SEND\ndestination:" + topic + "\n\n" + body;
            connectionHandler->sendFrame(stompFrame);
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            
        }
    } catch (const std::exception& e) {
        std::cout << "Error: Could not open or parse file: " << path << std::endl;
    }
}

void StompProtocol::runSocketListener() {
    std::string frame;
    // getFrame מחכה להודעה מהסוקט
    if (connectionHandler && connectionHandler->getFrame(frame)) {
        if (!frame.empty()) {
            // הדפסה זו קריטית כדי לראות את ה-RECEIPT של ה-logout
            std::cout << "\nFrame received from server:\n---\n" << frame << "\n---\n" << std::endl;
            processServerFrame(frame);
        }
    } else {
        // אם הסוקט נסגר (למשל השרת ניתק אותנו)
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
    std::string topic = normalizeTopic(game);
    std::ofstream out(file);
    if (!out.is_open()) return;

    std::string cleanGame = (game[0] == '/') ? game.substr(1) : game;
    size_t us = cleanGame.find('_');
    std::string teamA = cleanGame.substr(0, us);
    std::string teamB = cleanGame.substr(us + 1);

    out << teamA << " vs " << teamB << "\n";
    out << "Game stats:\n";

    std::map<std::string, std::string> generalStats;
    std::map<std::string, std::string> teamAStats;
    std::map<std::string, std::string> teamBStats;

    std::vector<GameEventReport> userEvents;

    if (gameReports.count(topic)) {
        for (const auto& e : gameReports[topic]) {
            if (e.user == user) {
                userEvents.push_back(e);

                for (const auto& kv : e.generalUpdates) generalStats[kv.first] = kv.second;
                for (const auto& kv : e.teamAUpdates)   teamAStats[kv.first]   = kv.second;
                for (const auto& kv : e.teamBUpdates)   teamBStats[kv.first]   = kv.second;
            }
        }
    }

    out << "General stats:\n";
    for (const auto& kv : generalStats)
        out << "    " << kv.first << ": " << kv.second << "\n";

    out << teamA << " stats:\n";
    for (const auto& kv : teamAStats)
        out << "    " << kv.first << ": " << kv.second << "\n";

    out << teamB << " stats:\n";
    for (const auto& kv : teamBStats)
        out << "    " << kv.first << ": " << kv.second << "\n";

    out << "Game event reports:\n";

    std::sort(userEvents.begin(), userEvents.end(),
              [](const GameEventReport& a, const GameEventReport& b) {
                  return a.time < b.time;
              });

    for (const auto& e : userEvents) {
        out << e.time << " - " << e.eventName << ":\n";
        out << e.description << "\n";
    }

    out.close();
    std::cout << "Summary generated in " << file << " for user " << user << std::endl;
}
