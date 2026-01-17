#pragma once

#include "../include/ConnectionHandler.h"
#include <string>
#include <vector>
#include <map>

using std::string;
using std::vector;
using std::map;

class StompProtocol {
private:
    ConnectionHandler* connectionHandler;
    bool isConnected;
    int subIdCounter;
    int receiptIdCounter;
    int disconnectReceiptId;
    bool shouldTerminate;
    
    string userName;
    map<string, int> topicToSubId;

    void disconnect();

public:
    StompProtocol();
    ~StompProtocol();

    bool connect(string host, short port, string user, string pass);

    // הלולאה שרצה ב-Thread הנפרד ומאזינה להודעות מהשרת
    void runSocketListener();

    // פונקציות לשליחת פקודות (מופעלות ע"י ה-Main)
    void sendJoin(string gameName);
    void sendExit(string gameName);
    void sendReport(string jsonFile);
    void sendLogout();

    bool getIsConnected() const;
    bool shouldTerminateClient() const;
};