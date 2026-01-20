#pragma once

#include "../include/ConnectionHandler.h"
#include <string>
#include <vector>
#include <map>

using std::string;
using std::vector;
using std::map;

// מבנה נתונים פנימי לשמירת אירועים שהתקבלו
struct GameEventReport {
    std::string user;
    std::string eventName;
    int time;
    std::string teamA;
    std::string teamB;
    std::string description; // השורה החסרה שגורמת לשגיאה
    std::map<std::string, std::string> updates;

    GameEventReport() : 
        user(""), 
        eventName(""), 
        time(0), 
        teamA(""), 
        teamB(""), 
        description(""),
        updates() 
    {}
};

class StompProtocol {
private:
   ConnectionHandler* connectionHandler;
    bool isConnected;
    int subIdCounter;
    int receiptIdCounter;
    int disconnectReceiptId;
    bool shouldTerminate;
    std::string userName;
    std::map<std::string, int> topicToSubId; // למעקב אחרי מנויים
    // הדאטאבייס המקומי: מפה בין שם ערוץ לרשימת האירועים שנשמרו בו
    std::map<std::string, std::vector<GameEventReport>> gameReports;
    std::map<int, std::string> receiptToCommand; // למיפוי ה-receipt לפעולה שבוצעה
    bool waitingForReceipt;                      // לדגל חסימת קלט מהמשתמש
   
    public:
    StompProtocol();
    virtual ~StompProtocol();
    StompProtocol(const StompProtocol&) = delete;
    StompProtocol& operator=(const StompProtocol&) = delete;

    bool connect(std::string host, short port, std::string user, std::string pass);
    void disconnect();
    bool getIsConnected() const;
    void runSocketListener(); // רץ בת'רד נפרד
    void saveSummary(std::string gameName, std::string user, std::string fileName);
    void processServerFrame(std::string frame); // מפרק הודעות מהשרת
    void processMessageBody(std::stringstream& bodyStream, std::string destination);
    bool shouldTerminateClient() const;
    bool isWaitingForReceipt() const;
  
    void sendJoin(string gameName);
    void sendExit(string gameName);
    void sendReport(string jsonFile);
    void sendLogout();

    
};