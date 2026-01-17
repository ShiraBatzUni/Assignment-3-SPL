package bgu.spl.net.impl.stomp;

import java.util.HashMap;
import java.util.Map;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.data.Database;
import bgu.spl.net.impl.data.LoginStatus;
import bgu.spl.net.srv.Connections;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {

    private int connectionId;
    private Connections<String> connections;
    private boolean shouldTerminate = false;
    private boolean isLoggedIn;
    private String currentUser;
    private Map<String, String> topics = new HashMap<>();
    private final Database database = Database.getInstance();

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public void process(String message) {
        Frame frame = Frame.parse(message);
        String command = frame.getCommand();
        switch (command) {
            case "CONNECT":
                handleConnect(frame);
                break;
            case "SEND":
                handleSend(frame);
                break;
            case "SUBSCRIBE":
                handleSubscribe(frame);
                break;
            case "UNSUBSCRIBE":
                handleUnsubscribe(frame);
                break;
            case "DISCONNECT":
                handleDisconnect(frame);
                break;
            default:
                sendError("Unknown Command", null);
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    private void handleConnect(Frame frame) {
        Map<String, String> headers = frame.getHeaders();
        String login = headers.get("login");
        String passcode = headers.get("passcode");

        if (login == null || passcode == null) {
            Frame errorFrame = Frame.error("Malformed Frame: Missing login or passcode", frame.toString());
            connections.send(connectionId, errorFrame.toString());
            shouldTerminate = true;
            return;
        }

        LoginStatus status = Database.getInstance().login(connectionId, login, passcode);

        switch (status) {
            case LOGGED_IN_SUCCESSFULLY:
            case ADDED_NEW_USER:
                this.isLoggedIn = true;
                this.currentUser = login;

                connections.send(connectionId, Frame.connected().toString());
                break;

            case WRONG_PASSWORD:
                connections.send(connectionId, Frame.error("Wrong password", null).toString());
                shouldTerminate = true;
                break;

            case ALREADY_LOGGED_IN:
                connections.send(connectionId, Frame.error("User already logged in", null).toString());
                shouldTerminate = true;
                break;

            case CLIENT_ALREADY_CONNECTED:
                connections.send(connectionId,
                        Frame.error("The client is already logged in, log out before trying again", null).toString());
                shouldTerminate = true;
                break;
        }
    }

    private void handleSend(Frame frame) {
        String destination = frame.getHeaders().get("destination");
        String receipt = frame.getHeaders().get("receipt");
        String body = frame.getBody();

        if (destination == null) {
            Frame error = Frame.error("Malformed Frame: No destination header", frame.toString());
            connections.send(connectionId, error.toString());
            shouldTerminate = true;
            return;
        }

        if (!topics.containsKey(destination)) {
            Frame error = Frame.error("User is not subscribed to topic " + destination, frame.toString());
            connections.send(connectionId, error.toString());
            shouldTerminate = true;
            return;
        }

        // 3. יצירת הודעת MESSAGE להפצה
        // השרת צריך לייצר message-id ייחודי לכל הודעה [cite: 98]
        // שימי לב: ה-subscription ID שנשלח ב-MESSAGE אמור להיות ה-ID של המקבל, לא
        // השולח.
        // בגלל מגבלות ה-Connections הנוכחי (שידור גורף), נשים כרגע ערך כללי או נטפל בזה
        // בשיפור ה-Connections בהמשך.

        // נניח שיש לך מונה סטטי במחלקה: private static AtomicInteger messageIdCounter =
        // new AtomicInteger(0);
        int msgId = messageIdCounter.incrementAndGet();

        Frame messageFrame = Frame.message(
                destination,
                "0", // הערה למטה: זה אמור להיות ה-Subscription ID של המקבל
                msgId,
                body,
                currentUser // שם המשתמש השולח חייב להופיע [cite: 384]
        );

        // 4. שליחה לכל המנויים בערוץ
        connections.send(destination, messageFrame.toString());

        // 5. שליחת קבלה (Receipt) אם התבקשה
        if (receipt != null) {
            connections.send(connectionId, Frame.receipt(receipt).toString());
        }
    }
}
