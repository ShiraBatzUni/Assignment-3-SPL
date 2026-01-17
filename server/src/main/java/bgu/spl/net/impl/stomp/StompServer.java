package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {

        if (args.length < 2) {
            System.out.println("Usage: StompServer <port> <tpc/reactor>");
            return;
        }

        int port = Integer.parseInt(args[0]);
        String serverType = args[1];

        if (serverType.equals("tpc")) {
            // --- הפעלת שרת Thread-Per-Client ---
            Server.threadPerClient(
                    port,
                    () -> new StompMessagingProtocolImpl(), // יצירת פרוטוקול
                    () -> new StompEncoderDecoder()         // תיקון: שם המחלקה ללא Message
            ).serve();

        } else if (serverType.equals("reactor")) {
            // --- הפעלת שרת Reactor ---
            Server.reactor(
                    Runtime.getRuntime().availableProcessors(),
                    port,
                    () -> new StompMessagingProtocolImpl(),
                    () -> new StompEncoderDecoder()         // תיקון: כנ"ל
            ).serve();

        } else {
            System.out.println("Unknown server type. Use 'tpc' or 'reactor'.");
        }
    }
}