#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <map>
#include <atomic>
#include <sstream>
#include "ConnectionHandler.h"

// הגדרת המבנה כאן פותרת את השגיאה ב-image_5bd2c4
struct GameEventReport {
    std::string user;
    std::string teamA;
    std::string teamB;
    std::string eventName;
    int time;
    std::map<std::string, std::string> updates;
    std::string description;

    GameEventReport() : user(""), teamA(""), teamB(""), eventName(""), time(0), updates(), description("") {}
};

class StompProtocol {
   private:
    int receiptCounter;
    std::unordered_map<int, std::string> pendingReceipts;
    std::unordered_map<int, std::string> topicIds; // מזהה מנוי -> שם הערוץ
    ConnectionHandler* connectionHandler;
    std::string currentUser; // לשמירת שם המשתמש הנוכחי לצורך ה-Summary
    std::atomic<bool> connected;
    std::atomic<bool> shouldTerminate;
    std::map<std::string, std::vector<GameEventReport>> gameReports;
public:
    StompProtocol();
    virtual ~StompProtocol();
    StompProtocol(const StompProtocol&) = delete;
    StompProtocol& operator=(const StompProtocol&) = delete;

    bool connect(std::string host, short port, std::string user, std::string pass);
    std::string processKeyboardCommand(const std::string& input);
    void saveSummary(std::string gameName, std::string user, std::string fileName);
    bool shouldTerminateClient() const;
    bool isConnectedToSocket() const;
    void runSocketListener();
    void sendJoin(std::string game);
    void sendLogout();
    void sendReport(std::string path);
   
private:
    // פונקציות אלו חייבות להיות מוצהרות כדי למנוע את השגיאות ב-image_5b5dc4
    void handleMessageFrame(std::string topic, std::string body);
    void processServerFrame(const std::string& frame);
    void processMessageBody(std::stringstream& bodyStream, std::string destination);
    void parseFrame(const std::string& frame, std::string& command, 
                    std::unordered_map<std::string, std::string>& headers, std::string& body);

};