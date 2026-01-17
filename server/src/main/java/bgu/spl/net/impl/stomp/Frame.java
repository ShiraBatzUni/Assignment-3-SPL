package bgu.spl.net.impl.stomp;

import java.util.*;

public class Frame {

    private final String command;
    private final Map<String, String> headers;
    private final String body;

    public Frame(String command, Map<String, String> headers, String body) {
        this.command = command;
        this.headers = headers;
        this.body = body;
    }

    public static Frame parse(String message) {
        String[] parts = message.split("\n\n", 2);
        String headerSection = parts[0];
        String body = parts.length > 1 ? parts[1] : "";
        if (body.endsWith("\0")) body = body.substring(0, body.length() - 1);

        String[] lines = headerSection.split("\n");
        String command = lines[0];
        Map<String, String> headers = new HashMap<>();

        for (int i = 1; i < lines.length; i++) {
            String[] kv = lines[i].split(":", 2);
            if (kv.length == 2) headers.put(kv[0], kv[1]);
        }

        return new Frame(command, headers, body);
    }

    public static Frame connected() {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("version", "1.2");
        return new Frame("CONNECTED", headers, "");
    }

    public static Frame receipt(String receiptId) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("receipt-id", receiptId);
        return new Frame("RECEIPT", headers, "");
    }

    public static Frame error(String message, String original) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("message", message);
        return new Frame("ERROR", headers, original == null ? "" : original);
    }

    public static Frame message(String destination, String subscription, int messageId, String body, String user) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("subscription", subscription);
        headers.put("message-id", String.valueOf(messageId));
        headers.put("destination", destination);
        headers.put("user", user);
        return new Frame("MESSAGE", headers, body);
    }

    public String getCommand() {
        return command;
    }

    public Map<String, String> getHeaders() {
        return headers;
    }

    public String getBody() {
        return body;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(command).append("\n");
        for (Map.Entry<String, String> entry : headers.entrySet()) {
            sb.append(entry.getKey()).append(":").append(entry.getValue()).append("\n");
        }
        sb.append("\n").append(body).append("\0");
        return sb.toString();
    }
}
