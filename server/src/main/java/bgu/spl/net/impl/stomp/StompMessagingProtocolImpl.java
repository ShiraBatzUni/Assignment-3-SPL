package bgu.spl.net.impl.stomp;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

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
    private static final AtomicInteger messageIdCounter = new AtomicInteger(0);

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
        String receiptId = frame.getHeaders().get("receipt");
        String body = frame.getBody();

        if (destination == null) {
            connections.send(connectionId, Frame.error("Malformed Frame", frame.toString()).toString());
            shouldTerminate = true;
            return;
        }

        if (!topics.containsKey(destination)) {
            connections.send(connectionId, Frame.error("User not subscribed", frame.toString()).toString());
            shouldTerminate = true;
            return;
        }

        Map<Integer, String> subscribers = ((ConnectionsImpl<String>) connections).getSubscribers(destination);

        if (subscribers != null) {
            int msgId = messageIdCounter.incrementAndGet();

            for (Map.Entry<Integer, String> entry : subscribers.entrySet()) {
                Integer targetConnectionId = entry.getKey();
                String targetAuthId = entry.getValue();
                Frame messageFrame = Frame.message(
                        destination,
                        targetAuthId,
                        msgId,
                        body,
                        currentUser);

                connections.send(targetConnectionId, messageFrame.toString());
            }
        }

        if (receiptId != null) {
            connections.send(connectionId, Frame.receipt(receiptId).toString());
        }
    }

    private void handleSubscribe(Frame frame) {
        String destination = frame.getHeaders().get("destination");
        String subId = frame.getHeaders().get("id");
        String receiptId = frame.getHeaders().get("receipt");

        if (destination == null || subId == null) {
            Frame error = Frame.error("Malformed Frame: Missing destination or id headers", frame.toString());
            connections.send(connectionId, error.toString());
            shouldTerminate = true;
            return;
        }

        topics.put(destination, subId);
        ((ConnectionsImpl<String>) connections).subscribe(destination, connectionId, subId);

        if (receiptId != null) {
            connections.send(connectionId, Frame.receipt(receiptId).toString());
        }
    }

    private void handleUnsubscribe(Frame frame) {
        String subId = frame.getHeaders().get("id");
        String receiptId = frame.getHeaders().get("receipt");

        if (subId == null) {
            connections.send(connectionId,
                    Frame.error("Malformed Frame: Missing id header", frame.toString()).toString());
            shouldTerminate = true;
            return;
        }

        String topicToRemove = null;
        for (Map.Entry<String, String> entry : topics.entrySet()) {
            if (entry.getValue().equals(subId)) {
                topicToRemove = entry.getKey();
                break;
            }
        }

        if (topicToRemove != null) {
            topics.remove(topicToRemove);
            ((ConnectionsImpl<String>) connections).unsubscribe(topicToRemove, connectionId);
            if (receiptId != null) {
                connections.send(connectionId, Frame.receipt(receiptId).toString());
            }
        } else {
            connections.send(connectionId, Frame.error("Subscription ID not found", frame.toString()).toString());
        }
    }

    private void handleDisconnect(Frame frame) {
        String receiptId = frame.getHeaders().get("receipt");
        if (receiptId != null) {
            connections.send(connectionId, Frame.receipt(receiptId).toString());
        }
        shouldTerminate = true;
        connections.disconnect(connectionId);
    }

    private void sendError(String message, String body) {
        Frame errorFrame = Frame.error(message, body);
        connections.send(connectionId, errorFrame.toString());
        shouldTerminate = true;
    }
}
