#include "../include/ConnectionHandler.h"

using boost::asio::ip::tcp;

ConnectionHandler::ConnectionHandler(std::string host, short port) : 
    host_(host), port_(port), io_service_(), socket_(io_service_) {}

ConnectionHandler::~ConnectionHandler() { close(); }

bool ConnectionHandler::connect() {
    try {
        tcp::endpoint endpoint(boost::asio::ip::address::from_string(host_), port_);
        socket_.connect(endpoint);
    } catch (std::exception& e) {
        return false;
    }
    return true;
}

/**
 * שליחת פריים (שאילתת SQL או פריים STOMP) לשרת.
 * מוודא שההודעה מסתיימת בתו NULL כפי שהשרת מצפה.
 */
bool ConnectionHandler::sendFrame(std::string& frame) {
   if (!sendBytes(frame.c_str(), frame.length())) {
        return false;
    }
    
    // שליחת תו ה-NULL המסיים במידה והוא לא נכלל באורך המחרוזת
    // ב-STOMP, כל פריים חייב להסתיים ב-'\0' (null character)
    char null_char = '\0';
    return sendBytes(&null_char, 1);
}

/**
 * קבלת פריים (תשובה מה-SQL) מהשרת.
 * משתמש ב-read_until כדי לקרוא את כל התוצאה עד לתו ה-NULL.
 */
bool ConnectionHandler::getFrame(std::string& frame) {
    try {
        boost::asio::streambuf buffer;
        boost::system::error_code error;
        
        // קריאה עד לתו ה-NULL שמסיים את תשובת ה-SQL
        boost::asio::read_until(socket_, buffer, '\0', error); 
        
        if (error) {
            return false;
        }

        std::istream is(&buffer);
        std::getline(is, frame, '\0'); 
        
        return true;
    } catch (std::exception& e) {
        std::cerr << "recv failed (Error: " << e.what() << ")" << std::endl;
        return false;
    }
}

/**
 * קריאת מספר בתים קבוע מהסוקט.
 */
bool ConnectionHandler::getBytes(char bytes[], unsigned int bytesToRead) {
    size_t tmp = 0;
    boost::system::error_code error;
    try {
        while (!error && bytesToRead > tmp) {
            tmp += socket_.read_some(boost::asio::buffer(bytes + tmp, bytesToRead - tmp), error);
        }
        if (error)
            throw boost::system::system_error(error);
    } catch (std::exception &e) {
        std::cerr << "recv failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

/**
 * שליחת מספר בתים קבוע לסוקט.
 */
bool ConnectionHandler::sendBytes(const char bytes[], int bytesToWrite) {
    int tmp = 0;
    boost::system::error_code error;
    try {
        while (!error && bytesToWrite > tmp) {
            tmp += socket_.write_some(boost::asio::buffer(bytes + tmp, bytesToWrite - tmp), error);
        }
        if (error)
            throw boost::system::system_error(error);
    } catch (std::exception &e) {
        std::cerr << "send failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

/**
 * פונקציות עזר לעבודה עם שורות טקסט (מבוססות n\)
 */
bool ConnectionHandler::getLine(std::string &line) {
    return getFrameAscii(line, '\n');
}

bool ConnectionHandler::sendLine(std::string &line) {
    return sendFrameAscii(line, '\n');
}

/**
 * קריאת טקסט עד לתו מפריד (Delimiter) מסוים.
 */
bool ConnectionHandler::getFrameAscii(std::string &frame, char delimiter) {
    char ch;
    try {
        do {
            if (!getBytes(&ch, 1)) {
                return false;
            }
            if (ch != '\0')
                frame.append(1, ch);
        } while (delimiter != ch);
    } catch (std::exception &e) {
        std::cerr << "recv failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

/**
 * שליחת טקסט ולאחריו תו מפריד.
 */
bool ConnectionHandler::sendFrameAscii(const std::string &frame, char delimiter) {
    bool result = sendBytes(frame.c_str(), frame.length());
    if (!result) return false;
    return sendBytes(&delimiter, 1);
}

// סגירת החיבור בצורה תקינה
void ConnectionHandler::close() {
    try {
        socket_.close();
    } catch (...) {
        std::cout << "closing failed: connection already closed" << std::endl;
    }
}