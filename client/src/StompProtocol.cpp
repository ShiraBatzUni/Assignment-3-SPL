#include "../include/StompProtocol.h"
#include "../include/event.h"
#include <sstream>
#include <iostream>

StompProtocol::StompProtocol() : 
    connectionHandler(nullptr), 
    isConnected(false), 
    subIdCounter(0), 
    receiptIdCounter(0), 
    disconnectReceiptId(-1), 
    shouldTerminate(false), 
    userName(""),
    topicToSubId() // הוספת המפה לרשימת האתחול פותרת את האזהרה
{
}

StompProtocol::~StompProtocol() {
    if (connectionHandler != nullptr) {
        delete connectionHandler;
        connectionHandler = nullptr;
    }
}

bool StompProtocol::shouldTerminateClient() const {
    return shouldTerminate;
}

// פונקציה למימוש פקודת ה-report מקובץ JSON [cite: 375]
void StompProtocol::sendReport(std::string jsonFile) {
    names_and_events nne = parseEventsFile(jsonFile); // שימוש בפרסר שסופק ב-event.cpp [cite: 514]
    std::string channel = nne.team_a_name + "_" + nne.team_b_name;

    for (const auto& event : nne.events) {
        // בניית פריים SEND עבור כל אירוע [cite: 380, 381]
        std::string frame = "SEND\ndestination:/" + channel + "\n\n";
        frame += "user: " + userName + "\n";
        frame += "team a: " + event.get_team_a_name() + "\n";
        frame += "team b: " + event.get_team_b_name() + "\n";
        frame += "event name: " + event.get_name() + "\n";
        frame += "time: " + std::to_string(event.get_time()) + "\n";
        // ... (המשך פירוט הסטטיסטיקות והתיאור לפי הפורמט בעמוד 16-17) [cite: 404]
        
        connectionHandler->sendFrame(frame);
    }
}

void StompProtocol::runSocketListener() {
    while (!shouldTerminate) {
        std::string answer;
        if (!connectionHandler->getFrame(answer)) break;
        // הדפסת תשובת השרת ועיבוד MESSAGE או RECEIPT [cite: 344, 372]
        std::cout << answer << std::endl;
        
        // בדיקה אם הגיע RECEIPT על Logout כדי לסגור את הלקוח [cite: 170, 479]
        if (answer.find("RECEIPT") != std::string::npos && 
            answer.find("receipt-id:" + std::to_string(disconnectReceiptId)) != std::string::npos) {
            shouldTerminate = true;
        }
    }
}