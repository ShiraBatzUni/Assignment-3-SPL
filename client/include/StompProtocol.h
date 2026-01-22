#pragma once
#include "../include/ConnectionHandler.h"
#include <string>
#include <unordered_map>
#include <map>
#include <vector>

struct GameEventReport {
    std::string user;
    std::string teamA;
    std::string teamB;
    std::string eventName;
    int time;
    std::string description;
    std::map<std::string, std::string> generalUpdates;
    std::map<std::string, std::string> teamAUpdates;
    std::map<std::string, std::string> teamBUpdates;

    GameEventReport()
        : user(""), teamA(""), teamB(""), eventName(""),
          time(0), description(""),
          generalUpdates(), teamAUpdates(), teamBUpdates() {}
};


class StompProtocol {
private:
    int receiptCounter;
    std::unordered_map<int, std::string> pendingReceipts;
    std::unordered_map<int, std::string> topicIds;
    ConnectionHandler* connectionHandler;
    std::string currentUser;
    bool connected;
    bool shouldTerminate;
    std::map<std::string, std::vector<GameEventReport>> gameReports;
    

    
   

public:
    StompProtocol();
    virtual ~StompProtocol();
    StompProtocol(const StompProtocol&) = delete;
    StompProtocol& operator=(const StompProtocol&) = delete;

    std::string extractTeamA(const std::string& gameName) const;
    std::string extractTeamB(const std::string& gameName) const;
    std::string normalizeTopic(std::string topic);
    bool connect(std::string host, short port, std::string user, std::string pass);
    void printSummary(const std::string& gameName, const std::string& user, const std::string& file);
    void processServerFrame(const std::string& frame);
    void handleMessageFrame(std::string topic, std::string body);
    void sendJoin(std::string game);
    void sendReport(std::string path);
    void sendLogout();
    void saveSummary(std::string game, std::string user, std::string file);
    void runSocketListener();
    bool shouldTerminateClient() const;
    bool isConnectedToSocket() const;
    void parseFrame(const std::string& frame, std::string& command, std::unordered_map<std::string, std::string>& headers, std::string& body);
};